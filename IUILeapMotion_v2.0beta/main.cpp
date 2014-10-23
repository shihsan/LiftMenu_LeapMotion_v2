//
//  main.cpp
//  IUILeapMotion_v2.0beta
//
//  Created by Carolyn on 2014/6/9.
//  Copyright (c) 2014年 Carolyn. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <thread>

#include <iostream>
#include "Leap.h"
int tapSettingDone;
float MaxVelocity;
float HighestFingerPosition;
float LowestFingerPosition;
long HighestFingerTimestamp;
long LowestFingerTimestamp;
float buf[4][4] = {0}; //tipVelocity, x, y, z
int refresh_buf = 1;
int stuck = 0;
long long tempTimestamp;
Leap::Vector keyTapPosition;

int sd;
struct sockaddr_in broadcastAddr;
std::ostringstream buf_out;
int key_tap = 0;
int ret;

using namespace Leap;

class SampleListener : public Listener {
public:
    virtual void onInit(const Controller&);
    virtual void onConnect(const Controller&);
    virtual void onDisconnect(const Controller&);
    virtual void onExit(const Controller&);
    virtual void onFrame(const Controller&);
    virtual void onFocusGained(const Controller&);
    virtual void onFocusLost(const Controller&);
};

void SampleListener::onInit(const Controller& controller) {
    std::cout << "Initialized" << std::endl;
}

void SampleListener::onConnect(const Controller& controller) {
    std::cout << "Connected" << std::endl;
    controller.enableGesture(Gesture::TYPE_CIRCLE);
    controller.enableGesture(Gesture::TYPE_KEY_TAP);
    controller.enableGesture(Gesture::TYPE_SCREEN_TAP);
    controller.enableGesture(Gesture::TYPE_SWIPE);
}

void SampleListener::onDisconnect(const Controller& controller) {
    //Note: not dispatched when running in a debugger.
    std::cout << "Disconnected" << std::endl;
}

void SampleListener::onExit(const Controller& controller) {
    std::cout << "Exited" << std::endl;
}

//----------------Socket Server Set up------------------------------------------------
void socket_setup(int port){
    
    int sockfd;
    int w;
    socklen_t adr_clnt_len;
    // struct sockaddr_in adr_inet;
    struct sockaddr_in adr_clnt;
    adr_clnt_len = sizeof(adr_clnt);
    int len;
    
    // Open a socket
    sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sd<=0) {
        perror("Error: Could not open socket");
    }
    
    
    // Set socket options
    // Enable broadcast
    int broadcastEnable=1;
    ret=setsockopt(sd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    if (ret) {
        perror("Error: Could not open set socket to broadcast mode");
        close(sd);
    }
    
    // Since we don't call bind() here, the system decides on the port for us, which is what we want.
    
    // Configure the port and ip we want to send to
    // Make an endpoint
    memset(&broadcastAddr, 0, sizeof broadcastAddr);
    broadcastAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &broadcastAddr.sin_addr); /*ios 模擬器（自己）127.0.0.1*//*（在device上測試時）169.254.255.255*/
    // Set the self broadcast IP address
    broadcastAddr.sin_port = htons(port); // Set port 8000
    
}
//------------------Socket Server Set up end----------------------------------

//------------------Gesture Setting and Detecting start-----------------------
void SampleListener::onFrame(const Controller& controller) {
    // Get the most recent frame and report some basic information
    const Frame frame = controller.frame();
    
    std::cout << "Frame id: " << frame.id()
    << ", timestamp: " << frame.timestamp()
    << ", hands: " << frame.hands().count()
    << ", fingers: " << frame.fingers().count()
    << ", tools: " << frame.tools().count()
    << ", gestures: " << frame.gestures().count() << std::endl;
    
    //clean buf_out, buf_out is the buf for transmitting out
    buf_out.str("");
    buf_out.clear();
    
    
    //Set new keytap configuration
    if(tapSettingDone){
        controller.config().setFloat("Gesture.KeyTap.MinDistance", 2.0f);
        
        if ( MaxVelocity > -50 ){ // if the velocity is slower than 50mm/s, reset the MaxVelocity
            controller.config().setFloat("Gesture.KeyTap.MinDownVelocity", fabs(MaxVelocity));
        }
        
        if ( fabs(LowestFingerTimestamp - HighestFingerTimestamp)/1000000 > 0.1) {
            // if the fingerTimestamp is longer then 0.1mm/s, reset the FingerHistorySeconds
            controller.config().setFloat("Gesture.KeyTap.HistorySeconds", fabs(LowestFingerTimestamp - HighestFingerTimestamp)/1000000);
        }
        controller.config().save();
    }
    key_tap = 0;
    
    // Get gestures
    const GestureList gestures = frame.gestures();
    for (int g = 0; g < gestures.count(); ++g) {
        Gesture gesture = gestures[g];
        
        switch (gesture.type()) {
                //            case Gesture::TYPE_CIRCLE:
                //            {
                //                CircleGesture circle = gesture;
                //                std::string clockwiseness;
                //
                //                if (circle.pointable().direction().angleTo(circle.normal()) <= PI/4) {
                //                    clockwiseness = "clockwise";
                //                } else {
                //                    clockwiseness = "counterclockwise";
                //                }
                //
                //                // Calculate angle swept since last frame
                //                float sweptAngle = 0;
                //                if (circle.state() != Gesture::STATE_START) {
                //                    CircleGesture previousUpdate = CircleGesture(controller.frame(1).gesture(circle.id()));
                //                    sweptAngle = (circle.progress() - previousUpdate.progress()) * 2 * PI;
                //                }
                //                std::cout << "Circle id: " << gesture.id()
                //                << ", state: " << gesture.state()
                //                << ", progress: " << circle.progress()
                //                << ", radius: " << circle.radius()
                //                << ", angle " << sweptAngle * RAD_TO_DEG
                //                <<  ", " << clockwiseness << std::endl;
                //                break;
                //            }
                //            case Gesture::TYPE_SWIPE:
                //            {
                //                SwipeGesture swipe = gesture;
                //                std::cout << "Swipe id: " << gesture.id()
                //                << ", state: " << gesture.state()
                //                << ", direction: " << swipe.direction()
                //                << ", speed: " << swipe.speed() << std::endl;
                //                break;
                //            }
            case Gesture::TYPE_KEY_TAP:
            {
                KeyTapGesture tap = gesture;
                std::cout << "*********** Key Tap id: " << gesture.id()
                << ", state: " << gesture.state()
                << ", position: " << tap.position()
                << ", direction: " << tap.direction()<< std::endl;
                
                keyTapPosition = tap.position();
                
                key_tap = 1;
                printf("open\n");
                //                if(key_tap){
                //                    int i;
                //                    for (i=3; i>=0; i--) {
                //                        if (buf[i][3] > -100 && buf[i][3] < 100) {
                //
                //                        }
                //                    }
                //                }
                
                break;
            }
                //            case Gesture::TYPE_SCREEN_TAP:
                //            {
                //                ScreenTapGesture screentap = gesture;
                //                std::cout << "Screen Tap id: " << gesture.id()
                //                << ", state: " << gesture.state()
                //                << ", position: " << screentap.position()
                //                << ", direction: " << screentap.direction()<< std::endl;
                //                break;
                //            }
            default:
                //                std::cout << "Unknown gesture type." << std::endl;
                break;
        }
    }
    
    //    if (!frame.hands().empty() || !gestures.empty()) {
    //        std::cout << std::endl;
    //    }
    
    //-------------Gesture Setting and Detectin end-----------------------
    
    
    //    if (!frame.hands().empty()) { //v1.0
    if (!frame.hands().isEmpty()) { //v2.0
        // Get the lower hand to the leap
        Hand hand = frame.hands()[0];
        
        //        //for API v1.0 lower hand detecter
        //        if(frame.hands().count() != 1){
        //            const Hand hand0 = frame.hands()[0];
        //            const Hand hand1 = frame.hands()[1];
        //            if(hand0.palmPosition().y >= hand1.palmPosition().y)
        //                hand = frame.hands()[1];
        //        }
        
        // Check if the hand has any fingers
        const FingerList fingers = hand.fingers();
        
        // if (!fingers.empty()) { //v1.0
        if (!fingers.isEmpty()) {//v2.0
            // Calculate the hand's average finger tip position
            Vector avgPos;
            for (int i = 0; i < fingers.count(); ++i) {
                avgPos += fingers[i].tipPosition();
            }
            avgPos /= (float)fingers.count();
            
            /* temp mark
             std::cout << "Hand has " << fingers.count()
             << " fingers, average finger tip position" << avgPos << std::endl;
             */
        }
        
        /* temp mark
         // Get the hand's sphere radius and palm position
         std::cout << "Hand sphere radius: " << hand.sphereRadius()
         << " mm, palm position: " << hand.palmPosition() << std::endl;
         */
        
        // Get the hand's normal vector and direction
        const Vector normal = hand.palmNormal();
        const Vector direction = hand.direction();

        
        /* temp mark
         // Calculate the hand's pitch, roll, and yaw angles
         std::cout << "Hand pitch: " << direction.pitch() * RAD_TO_DEG << " degrees, "
         << "roll: " << normal.roll() * RAD_TO_DEG << " degrees, "
         << "yaw: " << direction.yaw() * RAD_TO_DEG << " degrees" << std::endl;
         */
        
        
        //printf("高度: %f, 手指位置： %f", direction.pitch() * RAD_TO_DEG, fingers[0].tipPosition());
        // buf_out << fingers[0].tipPosition().y << ", " << fingers[0].tipPosition().x << ", " << fingers[0].tipPosition().z;
        
        //sprintf(buf, "高度: %f\n",direction.pitch() * RAD_TO_DEG );
        //std::string s = stringstream.str();
        //const char* p = s.c_str();
        
        
        //--------------refreshing refresh_buf  start-------------------
        
        //Close the refresh_buf and record the timestamp into tempTimestamp
        if(fingers[1].tipVelocity().y < -200 && abs(fingers[1].tipVelocity().y) > 2*abs(hand.palmVelocity().y) && refresh_buf){
            refresh_buf = 0;
            tempTimestamp = frame.timestamp();
            //            printf("Close\n");
        }
        
        //If no tap happened in 0.2 second, continue refresh the refresh_buf
        if (frame.timestamp() >= tempTimestamp+200000 && !key_tap) {
            refresh_buf = 1;
        }
        
        //Refreshing the refresh_buf
        if (refresh_buf && (fingers[1].tipPosition().y != 0 && fingers[1].tipPosition().x != 0 && fingers[1].tipPosition().z != 0)) {
            int i;
            for(i=0; i<3; i++){
                buf[i][0] = buf[i+1][0];
                buf[i][1] = buf[i+1][1];
                buf[i][2] = buf[i+1][2];
                buf[i][3] = buf[i+1][3];
            }
            buf[3][0] = fingers[1].tipPosition().y;
            buf[3][1] = fingers[1].tipPosition().x;
            buf[3][2] = fingers[1].tipPosition().z;
            buf[3][3] = fingers[1].tipVelocity().y;
            //            printf("Refreshing\n");
        }
        
        /* Painter Demo ------ status */
        if(fingers[1].tipPosition().y < 50)
            buf_out << "0, ";
        else if(fingers[1].tipPosition().y > 50 && fingers[1].tipPosition().y <100)
            buf_out << "1, ";
        else if(fingers[1].tipPosition().y > 100 && fingers[1].tipPosition().y <150)
            buf_out << "2, ";
        else if(fingers[1].tipPosition().y > 150)
            buf_out << "3, ";
       
         //buf_out << "z: " << fingers[1].tipPosition().y << " END ";
        
        //If stop refreshing and a key tap is detected, load the information that down velocity is between -0.1 and 0.1 into buf_out
        if (!refresh_buf && key_tap) {
            int i;
            stuck = 0;
            for (i=3; i>=0; i--) {
                if (buf[i][3] < 100 && buf[i][3] > -100) {
                    stuck = 1;
                    buf_out << fingers[1].tipPosition().y << ", " << buf[i][1] << ", " << buf[i][2];
                    //                    printf("buf: %f, %f, %f\n", buf[i][1], buf[i][2], buf[i][3]);
                    break;
                }
            }
            
            //no matching information in the refresh_buf, use the last information
            if(stuck == 0)
                buf_out << fingers[1].tipPosition().y << ", " << buf[3][1] << ", " << buf[3][2];
            
            // Continue refreshing
            refresh_buf = 1;
        }
        else{
            buf_out << fingers[1].tipPosition().y << ", " << fingers[1].tipPosition().x << ", " << fingers[1].tipPosition().z;
        }
        
        buf_out << ", "  << hand.palmPosition().y << ", " << hand.palmPosition().x << ", " << hand.palmPosition().z << ", " << key_tap << ", " << hand.pinchStrength();
        /* Painter Demo ------ END */
        
        /*  For Pin Test
        //If stop refreshing and a key tap is detected, load the information that down velocity is between -0.1 and 0.1 into buf_out
        if (!refresh_buf && key_tap) {
            int i;
            stuck = 0;
            for (i=3; i>=0; i--) {
                if (buf[i][3] < 100 && buf[i][3] > -100) {
                    stuck = 1;
                    buf_out << fingers[1].tipPosition().y << ", " << buf[i][1] << ", " << buf[i][2];
                    //                    printf("buf: %f, %f, %f\n", buf[i][1], buf[i][2], buf[i][3]);
                    break;
                }
            }
            
            //no matching information in the refresh_buf, use the last information
            if(stuck == 0)
                buf_out << fingers[1].tipPosition().y << ", " << buf[3][1] << ", " << buf[3][2];
            
            // Continue refreshing
            refresh_buf = 1;
        }
        else{
            buf_out << fingers[1].tipPosition().y << ", " << fingers[1].tipPosition().x << ", " << fingers[1].tipPosition().z;
        }
        //--------------refreshing refresh_buf  end-------------------
        
         //Recording the heighest and the lowest position
         if (!tapSettingDone && fabs(fingers[1].tipVelocity().y) > fabs(hand.palmVelocity().y)
         && !fingers.isEmpty()) { //v1.->v2.0 empty()->isEmpty()
         if(fingers[1].tipVelocity().y < MaxVelocity && fingers[1].tipVelocity().y < 0){
         MaxVelocity = fingers[1].tipVelocity().y;
         HighestFingerPosition = fingers[1].tipPosition().y;
         HighestFingerTimestamp = frame.timestamp();
         }
         //            if (fingers[0].tipPosition().y > HighestFingerPosition && fingers[0].tipVelocity().y < 0) {
         //                HighestFingerPosition = fingers[0].tipPosition().y;
         //                HighestFingerTimestamp = frame.timestamp();
         //            }
         if (fingers[1].tipPosition().y < LowestFingerPosition && fingers[1].tipVelocity().y < 0) {
         LowestFingerPosition = fingers[1].tipPosition().y;
         LowestFingerTimestamp = frame.timestamp();
         //                std::cout << "LowestTimestamp: " << LowestFingerTimestamp << std::endl;
         //                std::cout << buf_out.str() << std::endl;
         }
         }
         
         
         //--------------Save data to buf_out and transmit it to ios start-------------------
         //Data order: f_z, f_x, f_y, p_z, p_x, p_y, f_velocity, keytap, leap_keytap_x, leap_keytap_y, f_x(傳送當下), f_y(傳送當下)
         
         //Output palm position and tipVelocity and key_tap boolean to buf_out
         buf_out << ", " << hand.palmPosition().y << ", " << hand.palmPosition().x << ", " << hand.palmPosition().z << ", " << fingers[1].tipVelocity().y;
         
         buf_out << ",tap " << key_tap;
         
         //If key_tap is detected, also output keyTapPosition(Leap motion default position) to the buf_out
         if(key_tap){
         buf_out << ", " << keyTapPosition.x << ", " << keyTapPosition.z;
         buf_out << ", " << fingers[1].tipPosition().x << ", " << fingers[1].tipPosition().z;
         }else{     //if no keytap is detected
         buf_out << ", 0, 0, 0, 0";
         }
         //buf_out << ", " << hand.pinchStrength();   //for pinch
         */

    }
    else{   //if no hand is detected
        
        //buf_out << "0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0";
        //buf_out << "0, 0, 0, 0, 0, 0, 0,tap 0, 0, 0, 0, 0, 0";  //for pinch
        buf_out << "0, 0, 0, 0, 0, 0, 0, 0, 0";  //for Painter Demo
    }
    
    
     
        
    socket_setup(8000);
    
    // Send buf_out to ios device by broadcasting
    ret = sendto(sd, buf_out.str().c_str(), strlen(buf_out.str().c_str()), 0, (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
    std::cout << buf_out.str() << std::endl;
    
    if (ret<0) {
        perror("Error: Could not open send broadcast");
        close(sd);
    }
    close(sd);
    //--------------Save data to buf_out and transmit it to ios end-------------------
    
}

void SampleListener::onFocusGained(const Controller& controller) {
    std::cout << "Focus Gained" << std::endl;
}

void SampleListener::onFocusLost(const Controller& controller) {
    std::cout << "Focus Lost" << std::endl;
}

int main() {
    
    
    // Create a sample listener and controller
    SampleListener listener_transmitting;
    Controller controller;
    
    SampleListener listener_tapSetting;
    
    tapSettingDone = 0;
    MaxVelocity = 0;
    HighestFingerPosition = 0;
    LowestFingerPosition = 1000;
    int input;
    
    while (scanf("%d", &input) == 1) {
        
        //Mode 1: tap Setting Mode
        if (input == 1) {
            std::cout << "Start Setting KeyTap!\n";
            tapSettingDone = 0;
            MaxVelocity = 0;
            HighestFingerPosition = 0;
            LowestFingerPosition = 1000;
            // Have the sample listener receive events from the controller
            controller.addListener(listener_tapSetting);
            printf("tapSetting......\n");
        }
        
        
        //Mode 2: Print the new tapVelocity and historySeconds, and end Mode 1
        else if(input == 2){
            // Remove the sample listener when done
            controller.removeListener(listener_tapSetting);
            std::cout << "HighestTapVelocity: " << MaxVelocity << std::endl;
            std::cout << "HistorySeconds: " << fabs(LowestFingerTimestamp - HighestFingerTimestamp)/1000000 <<std::endl;
            tapSettingDone = 1;
        }
        
        
        //Mode 3
        else if(input == 3){
            std::cout << "Start Transmitting!\n";
            controller.addListener(listener_transmitting);
            
        }
    }
    
    
    return 0;
}


