#include "ofMain.h"
#include "ofxMotionBlurCamera.h"

class Box {
    ofColor color;
    float velocity;
    ofVec3f pos;
    ofQuaternion rot;
    ofMatrix4x4 mat;
    ofVec3f rotationAxis;
    float size;
public:
    Box(ofColor c, ofVec3f pos, ofQuaternion rot, float v, float sz)
    : color(c), pos(pos), rot(rot), size(sz)
    {
        velocity = v;
        rotationAxis = rot * ofVec3f(0,0,1);
        size = sz;
    }
    
    void update()
    {
        ofQuaternion nrot(velocity, rotationAxis);
        rot *= nrot;
        mat.setTranslation(pos);
        mat.setRotate(rot);
    }
    
    void draw()
    {
        ofPushMatrix();
        ofMultMatrix(mat);
        ofPushStyle();
        ofSetColor(color);
        ofDrawBox(size);
        ofPopStyle();
        ofPopMatrix();
    }
};

//========================================================================
static const int NUM_BOXES = 300;
class ofApp : public ofBaseApp{
    ofxMotionBlurCamera cam;
    ofEasyCam ecam;
    ofLight light0;
    
    vector<Box> boxes;
    
    bool bStop;
    bool bEasyCam;
    bool bSideBySide;
    
    ofVboMesh m;
public:
    void setup()
    {
        ofSetVerticalSync(true);
        ofSetFrameRate(60);
        
        cam.setup(1280, 720, GL_RGB);
        cam.setGlobalPosition(ofVec3f(0, 0, 0));
        cam.setNearClip(1);
        cam.setFarClip(3000);
        cam.setExposureTime(0.05);
        cam.setS(17); // smaller is faster.  9 for typical realtime gaming
        
        for (int j=0; j<NUM_BOXES; ++j) {
            float v = ofRandom(0.1, 1.0);
            ofColor c = ofColor::fromHsb(ofRandom(0, 255), 64, ofRandom(160, 255));
            
            // xy ring
            ofVec3f pos;
            {
                float z = ofRandom(-500, 500);
                float r = ofRandom((z < -400 || z > 400) ? 0 : 200, 500);
                float angle = ofRandom(ofDegToRad(360));
                pos = ofVec3f(r * cos(angle), r * sin(angle), z);
            }
            ofQuaternion rot;
            rot *= ofQuaternion(ofRandom(360), ofVec3f(1,0,0));
            rot *= ofQuaternion(ofRandom(360), ofVec3f(0,1,0));
            rot *= ofQuaternion(ofRandom(360), ofVec3f(0,0,1));
            
            boxes.push_back(Box(c, pos, rot, v, ofRandom(30, 70)));
        }
        
        ofSpherePrimitive p(1000, 24);
        m = p.getMesh();
        
        bEasyCam = false;
        bStop = false;
        bSideBySide = false;
    }
    
    void updateInternal()
    {
        if (bStop) {
            return;
        }

        for (Box& box : boxes) {
            box.update();
        }

        if (bEasyCam) {
            ecam.begin();
            ecam.end();
            cam.setTransformMatrix(ecam.getGlobalTransformMatrix());
        } else {
            int frame = ofGetFrameNum();
            
            float t = ofGetElapsedTimef();
            float sigm = (tanhf(2.5 * ofSignedNoise(3. * t)) + 1.0) / 2.0;
            float fov = ofMap(sigm, -1, 1, 10, 80, true);
            cam.setFov(fov);

            ofQuaternion rot = cam.getGlobalOrientation();
            float nx = ofSignedNoise(1,0,0,t);
            float ny = ofSignedNoise(0,1,0,t);
            rot *= ofQuaternion(ofMap(nx*nx*nx, -1, 1, -10, 10, true), ofVec3f(1,0,0));
            rot *= ofQuaternion(ofMap(ny*ny*ny, -1, 1, -10, 10, true), ofVec3f(0,1,0));
            cam.setGlobalOrientation(rot);
            
            float sigmz = (tanhf(2.5 * ofSignedNoise(0, 0, 1, t)) + 1.0) / 2.0;
            float z = 500.0 * ofClamp(sigmz, -1, 1);
            cam.setGlobalPosition(0, 0, z);
        }
    }
    
    void drawScene()
    {
        ofClear(0);
        
        ofEnableDepthTest();
        ofEnableLighting();
        ofEnableBlendMode(OF_BLENDMODE_ALPHA);
        
        ofPushStyle();
        ofSetLineWidth(3);
        ofSetColor(192);
        m.drawWireframe();
        ofPopStyle();
        
        light0.enable();
        light0.setPosition(100, 100, -300);

        for (Box& box : boxes) {
            box.draw();
        }
        ofDisableBlendMode();
        ofDisableDepthTest();
        ofDisableLighting();
    }

    void draw()
    {
        ofClear(0);
        
        cam.begin();
        drawScene();
        cam.end(false, !bStop);
        
        if (bSideBySide) {
            cam.getFbo().draw(0, 0, 854, 480);
            cam.getRawFbo().draw(0, 480, 854, 480);
            ofDrawBitmapStringHighlight("With Motion Blur", 10, 470);
            ofDrawBitmapStringHighlight("Without Motion Blur", 10, 950);
        } else {
            cam.draw(0, 0);
            cam.debugDraw();
        }

        
        // TODO: move it into update
        updateInternal();
        
        {
            stringstream ss;
            ss << "fps         : " << ofGetFrameRate() << endl;
            ss << "exposure    : " << cam.getExposureTime() << endl;
            ss << "samples     : " << cam.getS() << endl;
            ss << "-- Key Map --" << endl;
            ss << "[space] : Toggle Pause" << endl;
            ss << "[e] : Toggle EasyCam" << endl;
            ss << "[s] : Toggle Side By Side View";
            ofDrawBitmapStringHighlight(ss.str(), 10, 20);

        }
    }
    
    void keyPressed(int key)
    {
        if (key == ' ') {
            bStop = !bStop;
        }
        if (key == 'e') {
            bEasyCam = !bEasyCam;
        }
        if (key == 's') {
            bSideBySide = !bSideBySide;
        }
    }
};

//========================================================================
int main( ){
    ofSetupOpenGL(1280,960,OF_WINDOW);            // <-------- setup the GL context
    
    // this kicks off the running of my app
    // can be OF_WINDOW or OF_FULLSCREEN
    // pass in width and height too:
    ofRunApp(new ofApp());
    
}
