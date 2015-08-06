//
// Created by Yuya Hanai, https://github.com/hanasaan
//
#include "ofxMotionBlurCamera.h"

#define STRINGIFY(A) #A

static inline void setupShaderWithHeader(ofShader& s, const string& str)
{
    stringstream ss;
    ss << "#version 120" << endl;
    ss << "#extension GL_EXT_gpu_shader4 : enable" << endl;
    ss << str;
    s.setupShaderFromSource(GL_FRAGMENT_SHADER, ss.str());
    s.linkProgram();
}

ofxMotionBlurCamera::ofxMotionBlurCamera() :
    exposure_time(0.03f), S(9), K(20) {}

void ofxMotionBlurCamera::setup(int width, int height, int internalformat, float k)
{
    K = k;
    // allocate main fbo
    ofFbo::Settings s;
    s.internalformat = internalformat;
    s.width = width;
    s.height = height;
    s.useDepth = true;
    s.useStencil = false;
    s.depthStencilInternalFormat = GL_DEPTH_COMPONENT24;
    s.depthStencilAsTexture = true;
    fbo_main.allocate(s);
    
    fbo_velocity.allocate(width, height, GL_RG8);
    fbo_tile_max.allocate(width / k, height / k, GL_RG8);
    fbo_neighbor_max.allocate(width / k, height / k, GL_RG8);
    fbo_tile_max.getTextureReference().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
    fbo_neighbor_max.getTextureReference().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
    fbo_out.allocate(width, height, internalformat);
    
    string velocity_frag_shader_str = STRINGIFY
    (
     uniform sampler2DRect tex; // depth texture
     uniform sampler2DRect texMain; // color texture
     uniform mat4 postTransformMatrix;
     uniform vec2 sz;
     uniform float cameraNear;
     uniform float cameraFar;

     float cameraFarPlusNear = cameraFar + cameraNear;
     float cameraFarMinusNear = cameraFar - cameraNear;
     float cameraCoef = 2.0 * cameraNear;
     
     vec2 halfSz = sz * 0.5;
     vec2 invHalfSz = vec2(1.0 / halfSz.x, 1.0 / halfSz.y);
     
     // RGBA depth
     float unpackDepth( const in vec4 rgba_depth ) {
         return 2.0 * rgba_depth.x - 1.0;//depth;
     }
     
     float readDepth( const in vec2 coord ) {
         return cameraCoef / ( cameraFarPlusNear - unpackDepth( texture2DRect( tex, coord ) ) * cameraFarMinusNear );
     }
     

     // postTransformMatrix = (prev_projection * prev_camera_view) * inv(current_projection * current_camera_view)
     void main()
     {
         // this is current projected screen position, in OpenGL coordinates
         vec4 currentPosition;
         currentPosition.xy = gl_TexCoord[0].st * invHalfSz - vec2(1.0); // normalize to -1.0 to 1.0
         currentPosition.z = unpackDepth(texelFetch2DRect(tex, ivec2(gl_TexCoord[0].x, gl_TexCoord[0].y)));
         currentPosition.w = 1.0;
         
         float currentPositionEyeZ = cameraFar * readDepth(gl_TexCoord[0].st);
         vec4 currentPositionBeforeNormalized = currentPosition;
         currentPositionBeforeNormalized *= vec4(-currentPositionEyeZ);
         
         vec4 prevPosition = postTransformMatrix * currentPositionBeforeNormalized;
         prevPosition.xyz = prevPosition.xyz / prevPosition.w;
         
         // half spread velocity
         vec2 velocity;
         if (texture2DRect(texMain, gl_TexCoord[0].st).a < 0.1) {
             velocity = vec2(127.0/255.0);
         } else {
             velocity = (currentPosition.xy - prevPosition.xy) * 0.5;
             velocity.y = -velocity.y;
             // velocity encoding :
             // http://www.crytek.com/download/Sousa_Graphics_Gems_CryENGINE3.pdf
             velocity = sign(velocity) * sqrt(abs(velocity)) * 127.0 / 255.0 + vec2(127.0/255.0);
         }
         
//         gl_FragColor = vec4(readDepth(gl_TexCoord[0].st), readDepth(gl_TexCoord[0].st), 0.0, 1.0); // velocity
         gl_FragColor = vec4(velocity.x, velocity.y, 0.0, 1.0); // velocity
     }
     );
    
    string tile_max_frag_shader_str = STRINGIFY
    (
     uniform sampler2DRect tex;  // This is reference size texture
     uniform sampler2DRect texVelocity; // This is velocity texture
     uniform float k;
     void main() {
         int startx = int(gl_TexCoord[0].x) * int(k);
         int starty = int(gl_TexCoord[0].y) * int(k);
         int endx = startx + int(k) - 1;
         int endy = starty + int(k) - 1;
         float velocitymax = 0.0;
         vec2 velocitymaxvec = vec2(127.0/255.0);
         for (int y=starty; y<=endy; ++y) {
             for (int x=startx; x<=endx; ++x) {
                 vec2 vvec = texelFetch2DRect(texVelocity, ivec2(x,y)).xy;
                 vec2 vvecdec = (vvec - vec2(127.0/255.0));
                 float v = length(vvecdec);
                 if (v > velocitymax) {
                     velocitymax = v;
                     velocitymaxvec = vvec;
                 }
             }
         }
         gl_FragColor = vec4(velocitymaxvec.x, velocitymaxvec.y, 0.0, 1.0);
     }
     );
    
    string neighbor_max_frag_shader_str = STRINGIFY
    (
     uniform sampler2DRect tex;
     void main() {
         int u = int(gl_TexCoord[0].x);
         int v = int(gl_TexCoord[0].y);
         float velocitymax = 0.0;
         vec2 velocitymaxvec = vec2(127.0/255.0);
         for (int y=v-1; y<=v+1; ++y) {
             for (int x=u-1; x<=u+1; ++x) {
                 vec2 vvec = texture2DRect(tex, vec2(x,y)).xy;
                 vec2 vvecdec = (vvec - vec2(127.0/255.0));
                 float v = length(vvecdec);
                 if (v > velocitymax) {
                     velocitymax = v;
                     velocitymaxvec = vvec;
                 }
             }
         }
         gl_FragColor = vec4(velocitymaxvec.x, velocitymaxvec.y, 0.0, 1.0);
     }
     );
    
    string reconstruct_frag_shader_str = STRINGIFY
    (
     uniform sampler2DRect tex;
     uniform sampler2DRect texVelocity;
     uniform sampler2DRect texDepth;
     uniform sampler2DRect neighborMax;
     uniform float k;
     uniform float cameraNear;
     uniform float cameraFar;
     uniform int S;
     uniform vec2 viewport;
     uniform float exposureTime;
     uniform float fps;
     
     float cameraFarPlusNear = cameraFar + cameraNear;
     float cameraFarMinusNear = cameraFar - cameraNear;
     float cameraCoef = 2.0 * cameraNear;
     
     const float SOFT_Z_EXTENT = 0.1;
     
     // RGBA depth
     float unpackDepth( const in vec4 rgba_depth ) {
         return 2.0 * rgba_depth.x - 1.0;//depth;
     }
     
     float readDepth( const in vec2 coord ) {
         vec4 rgba = texture2DRect( texDepth, coord );
         return cameraCoef / ( cameraFarPlusNear - unpackDepth( rgba ) * cameraFarMinusNear );
     }
     
     // convert to pixel space
     vec2 decodeVelocity(const in vec2 v) {
         vec2 vd = v;
         vd = (vd - vec2(127.0/255.0));
         vd = (vd * vd) * sign(vd) * viewport;
         
         // clamp
         float length = clamp(exposureTime * fps * length(vd), 0.0, k * 3.0);
         vd = normalize(vd) * length;
         
         return vd;
     }
     
     float rand(vec2 co) {
         return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
     }
     
     float softDepthCompare(float za, float zb) {
         return clamp(1.0 - (za - zb)/SOFT_Z_EXTENT, 0.0, 1.0);
     }
     
     float cone(vec2 x, vec2 y, vec2 v) {
         return clamp(1.0 - length(x-y)/length(v), 0.0, 1.0);
     }
     
     float cylinder(vec2 x, vec2 y, vec2 v) {
         return 1.0 - smoothstep(0.95*length(v), 1.05*length(v), length(x-y));
     }
     
     void main() {
         vec2 uv = gl_TexCoord[0].xy;
         int x = int(uv.x / k);
         int y = int(uv.y / k);
         vec2 vmax = decodeVelocity(texelFetch2DRect(neighborMax, ivec2(x,y)).xy);
         if (length(vmax) < 0.5) {
             gl_FragColor = texture2DRect(tex, uv);
             return;
         }
         vec2 v = decodeVelocity(texelFetch2DRect(texVelocity, ivec2(uv)).xy);
         float lv = clamp(length(v), 1.0, k);
         float weight = 1.0 / lv;
         vec4 sum = texture2DRect(tex, uv) * weight;
         float j = -0.5 + 1.0 * rand(uv);
         vec2 X = uv;
         float zx = cameraFar * readDepth(uv);
         for (int i=0; i<S; ++i) {
             if (i==((S-1)/2)) {
                 continue;
             }
             float t = mix(-1.0, 1.0, (i + j + 1.0) / (S + 1.0));
             vec2 Y = floor(uv + vmax * t + vec2(0.5));
//             float zx = farClip * texelFetch2DRect(normalDepth, ivec2(uv)).a;
//             float zy = farClip * texelFetch2DRect(normalDepth, ivec2(Y)).a;
             
             float zy = cameraFar * readDepth(Y);
             vec2 vy = decodeVelocity(texelFetch2DRect(texVelocity, ivec2(Y)).xy);
             
             float f = softDepthCompare(zx, zy);
             float b = softDepthCompare(zy, zx);
             float ay = b * cone(X, Y, vy) + f * cone(X, Y, v) + cylinder(Y, X, vy) * cylinder(X, Y, v) * 2.0;
             weight += ay;
             sum += texture2DRect(tex, Y) * ay;
         }
         gl_FragColor = sum / weight;
     }
     );
    
    setupShaderWithHeader(velocity_shader, velocity_frag_shader_str);
    setupShaderWithHeader(tile_max_shader, tile_max_frag_shader_str);
    setupShaderWithHeader(neighbor_max_shader, neighbor_max_frag_shader_str);
    setupShaderWithHeader(reconstruction_shader, reconstruct_frag_shader_str);
}

void ofxMotionBlurCamera::begin(ofRectangle viewport)
{
    fbo_main.begin();
    ofColor background = ofGetStyle().bgColor;
    ofClear(background.r, background.g, background.b, 0);
    
    ofCamera::begin(viewport);
    
    ofPushStyle();
    glPushAttrib(GL_ENABLE_BIT);
}

void ofxMotionBlurCamera::end()
{
    end(true);
}

void ofxMotionBlurCamera::end(bool autodraw, bool update_prev)
{
    glPopAttrib();
    ofPopStyle();
    
    ofMatrix4x4 current_pmv = getModelViewProjectionMatrix(ofRectangle(0, 0, getWidth(), getHeight()));
    
    ofCamera::end();
    fbo_main.end();
    
    // postTransformMatrix = (prev_projection * prev_camera_view) * inv(current_projection * current_camera_view)

    // Veclocity
    ofMatrix4x4 post_transform = current_pmv.getInverse() * prev_pmv;
    fbo_velocity.begin();
    velocity_shader.begin();
    velocity_shader.setUniform1f("cameraFar", getFarClip());
    velocity_shader.setUniform1f("cameraNear", getNearClip());
    velocity_shader.setUniformMatrix4f("postTransformMatrix", post_transform);
    velocity_shader.setUniform2f("sz", getWidth(), getHeight());
    velocity_shader.setUniformTexture("texMain", fbo_main.getTextureReference(), 1);
    fbo_main.getDepthTexture().draw(0, 0);
    velocity_shader.end();
    fbo_velocity.end();
    
    // Tile Max
    fbo_tile_max.begin();
    tile_max_shader.begin();
    tile_max_shader.setUniformTexture("texVelocity", fbo_velocity.getTextureReference(), 1);
    tile_max_shader.setUniform1f("k", K);
    fbo_neighbor_max.getTextureReference().draw(0, 0);
    tile_max_shader.end();
    fbo_tile_max.end();
    
    // Neighbor Max
    fbo_neighbor_max.begin();
    neighbor_max_shader.begin();
    fbo_tile_max.draw(0, 0);
    neighbor_max_shader.end();
    fbo_neighbor_max.end();
    
    // Render final fbo
    fbo_out.begin();
    ofClear(0);
    reconstruction_shader.begin();
    reconstruction_shader.setUniform1f("cameraFar", getFarClip());
    reconstruction_shader.setUniform1f("cameraNear", getNearClip());
    reconstruction_shader.setUniform1f("k", K);
    reconstruction_shader.setUniform1i("S", S);
    reconstruction_shader.setUniform1f("exposureTime", exposure_time);
    reconstruction_shader.setUniform1f("fps", ofGetFrameRate());
    reconstruction_shader.setUniform2f("viewport", getWidth(), getHeight());
    reconstruction_shader.setUniformTexture("texVelocity", fbo_velocity.getTextureReference(), 1);
    reconstruction_shader.setUniformTexture("texDepth", fbo_main.getDepthTexture(), 2);
    reconstruction_shader.setUniformTexture("neighborMax", fbo_neighbor_max.getTextureReference(), 3);
    
    fbo_main.draw(0, 0);
    reconstruction_shader.end();
    fbo_out.end();

    if (autodraw) {
        draw(0, 0);
    }
    if (update_prev) {
        prev_pmv = current_pmv;
    }
}

void ofxMotionBlurCamera::debugDraw()
{
    ofPushStyle();

    ofDisableAlphaBlending();
    float w2 = getWidth();
    float h2 = getHeight();
    float ws = w2*0.25;
    float hs = h2*0.25;
    float h = ofGetViewportHeight() - hs;
    
    fbo_main.getDepthTexture().draw(0, h, ws, hs);
    fbo_velocity.getTextureReference().draw(ws, h, ws, hs);
    fbo_tile_max.getTextureReference().draw(ws*2, h, ws, hs);
    fbo_neighbor_max.getTextureReference().draw(ws*3, h, ws, hs);

    ofPopStyle();
}
