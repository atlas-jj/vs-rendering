/**
* This file is part of ORB-SLAM2.
*
* Copyright (C) 2014-2016 Raúl Mur-Artal <raulmur at unizar dot es> (University of Zaragoza)
* For more information see <https://github.com/raulmur/ORB_SLAM2>
*
* ORB-SLAM2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2. If not, see <http://www.gnu.org/licenses/>.
*/

#include "Viewer.h"
#include <pangolin/pangolin.h>

#include <mutex>

namespace ORB_SLAM2
{

    Viewer::Viewer(System* pSystem, FrameDrawer *pFrameDrawer, MapDrawer *pMapDrawer,  ModelDrawer* pModelDrawer,
                   Tracking *pTracking, const string &strSettingPath):
            mpSystem(pSystem), mpFrameDrawer(pFrameDrawer), mpMapDrawer(pMapDrawer), mpModelDrawer(pModelDrawer),
            mpTracker(pTracking), mbFinishRequested(false), mbFinished(true), mbStopped(true), mbStopRequested(false)
    {
        cv::FileStorage fSettings(strSettingPath, cv::FileStorage::READ);

        float fps = fSettings["Camera.fps"];
        if(fps<1)
            fps=30;
        mT = 1e3/fps;

        mImageWidth = fSettings["Camera.width"];
        mImageHeight = fSettings["Camera.height"];
        if(mImageWidth<1 || mImageHeight<1)
        {
            mImageWidth = 640;
            mImageHeight = 480;
        }

        mViewpointX = fSettings["Viewer.ViewpointX"];
        mViewpointY = fSettings["Viewer.ViewpointY"];
        mViewpointZ = fSettings["Viewer.ViewpointZ"];
        mViewpointF = fSettings["Viewer.ViewpointF"];

        mfx = fSettings["Camera.fx"];
        mfy = fSettings["Camera.fy"];
        mcx = fSettings["Camera.cx"];
        mcy = fSettings["Camera.cy"];

    }


    void Viewer::Run()
    {
        mbFinished = false;
        mbStopped = false;

//        pangolin::CreateWindowAndBind("ORB-SLAM2: Map Viewer",1024,768);
        pangolin::CreateWindowAndBind("ORB-SLAM2: Map Viewer",mImageWidth+175,mImageHeight);

        // 3D Mouse handler requires depth testing to be enabled
        glEnable(GL_DEPTH_TEST);

        // Issue specific OpenGl we might need
        glEnable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        pangolin::CreatePanel("menu").SetBounds(0.0,1.0,0.0,pangolin::Attach::Pix(175));
        pangolin::Var<bool> menuFollowCamera("menu.Follow Camera",true,true);
        pangolin::Var<bool> menuShowPoints("menu.Show Points",true,true);
        pangolin::Var<bool> menuShowKeyFrames("menu.Show KeyFrames",false,true);
        pangolin::Var<bool> menuShowGraph("menu.Show Graph",false,true);
        pangolin::Var<bool> menuCameraView("menu.Camera View",true,true);
        pangolin::Var<bool> menuShowModel("menu.Show Model",true,true);
        pangolin::Var<bool> menuShowTexture("menu.Show Texture",true,true);
        pangolin::Var<bool> menuShowLines("menu.Show Lines",true,true);
        pangolin::Var<bool> menuLocalizationMode("menu.Localization Mode",false,true);
        pangolin::Var<bool> menuReset("menu.Reset",false,false);

        // Define Camera Render Object (for view / scene browsing)
        pangolin::OpenGlRenderState s_map(
//                pangolin::ProjectionMatrix(1024,768,mViewpointF,mViewpointF,512,389,0.1,1000),
//                pangolin::ModelViewLookAt(mViewpointX,mViewpointY,mViewpointZ, 0,0,0,0.0,-1.0, 0.0)
                // carv: using calibrated camera center and focal length
                pangolin::ProjectionMatrix(mImageWidth,mImageHeight,mfx,mfy,mcx,mcy,0.1,1000),
                pangolin::ModelViewLookAt(0,0,0, 0,0,1, 0.0,-1.0, 0.0)
        );

        // Add named OpenGL viewport to window and provide 3D Handler
        pangolin::View& d_map = pangolin::CreateDisplay()
//                .SetBounds(0.0, 1.0, pangolin::Attach::Pix(175), 1.0, -1024.0f/768.0f)
                .SetBounds(0.0, 1.0, pangolin::Attach::Pix(175), 1.0, -mImageWidth/mImageHeight)
                .SetHandler(new pangolin::Handler3D(s_map));

        pangolin::OpenGlMatrix MapTwc;
        MapTwc.SetIdentity();

        cv::namedWindow("ORB-SLAM2: Current Frame");

        cv::namedWindow("Detected Lines");

        bool bFollow = true;
        bool bLocalizationMode = false;

        // carv: camera close up view
        bool bCameraView = true;
        pangolin::OpenGlMatrix projectionAbove = pangolin::ProjectionMatrix(mImageWidth,mImageHeight,mViewpointF,mViewpointF,
                                                                           mImageWidth/2,mImageHeight/2,0.1,1000);
        pangolin::OpenGlMatrix projectionCamera = pangolin::ProjectionMatrix(mImageWidth,mImageHeight,mfx,mfy,mcx,mcy,0.1,1000);
        pangolin::OpenGlMatrix viewAbove = pangolin::ModelViewLookAt(mViewpointX,mViewpointY,mViewpointZ, 0,0,0,0.0,-1.0, 0.0);
        pangolin::OpenGlMatrix viewCamera = pangolin::ModelViewLookAt(0,0,0, 0,0,1, 0.0,-1.0, 0.0);

        while(1)
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            mpMapDrawer->GetCurrentOpenGLCameraMatrix(MapTwc);

            if(menuFollowCamera && bFollow)
            {
                s_map.Follow(MapTwc);
            }
            else if(menuFollowCamera && !bFollow)
            {
//                s_map.SetModelViewMatrix(pangolin::ModelViewLookAt(mViewpointX,mViewpointY,mViewpointZ, 0,0,0,0.0,-1.0, 0.0));
                s_map.Follow(MapTwc);
                bFollow = true;
            }
            else if(!menuFollowCamera && bFollow)
            {
                bFollow = false;
            }

            if(menuLocalizationMode && !bLocalizationMode)
            {
                mpSystem->ActivateLocalizationMode();
                bLocalizationMode = true;
            }
            else if(!menuLocalizationMode && bLocalizationMode)
            {
                mpSystem->DeactivateLocalizationMode();
                bLocalizationMode = false;
            }

            // carv: setup viewpoint to see model
            if(menuCameraView && !bCameraView)
            {
                s_map.SetProjectionMatrix(projectionCamera);
                s_map.SetModelViewMatrix(viewCamera);
                bCameraView = true;
            }
            else if(!menuCameraView && bCameraView)
            {
                s_map.SetProjectionMatrix(projectionAbove);
                s_map.SetModelViewMatrix(viewAbove);
                bCameraView = false;
            }

            d_map.Activate(s_map);
            glClearColor(1.0f,1.0f,1.0f,1.0f);
            mpMapDrawer->DrawCurrentCamera(MapTwc);
            if(menuShowKeyFrames || menuShowGraph)
                mpMapDrawer->DrawKeyFrames(menuShowKeyFrames,menuShowGraph);
            if(menuShowPoints) {
//                mpMapDrawer->DrawMapPoints();
                // carv: show model points
                mpModelDrawer->DrawModelPoints();
            }

            CheckGlDieOnError()
            // carv: show model or triangle with light from camera
            if(menuShowModel && menuShowTexture) {
                mpModelDrawer->DrawModel();
            }
            else if (menuShowModel && !menuShowTexture) {
                mpModelDrawer->DrawTriangles(MapTwc);
            }
            else if (!menuShowModel && menuShowTexture) {
                mpModelDrawer->DrawFrame();
            }
            CheckGlDieOnError()

            pangolin::FinishFrame();

            //carv: show detected lines
            if(menuShowLines){
                cv::Mat im = mpModelDrawer->DrawLines();
                cv::imshow("Detected Lines",im);
            }

            cv::Mat im = mpFrameDrawer->DrawFrame();
            cv::imshow("ORB-SLAM2: Current Frame",im);
            cv::waitKey(mT);

            if(menuReset)
            {
                menuShowGraph = false;
                menuShowKeyFrames = false;
                menuShowPoints = true;
                menuLocalizationMode = false;
                if(bLocalizationMode)
                    mpSystem->DeactivateLocalizationMode();
                bLocalizationMode = false;
                bFollow = true;
                menuFollowCamera = true;
                // carv: reset to default
                menuCameraView = true;
                menuShowModel = true;
                menuShowTexture = true;
                menuShowLines = true;

                mpSystem->Reset();
                menuReset = false;
            }

            if(Stop())
            {
                while(isStopped())
                {
                    usleep(3000);
                }
            }

            if(CheckFinish())
                break;
        }

        SetFinish();
    }

    void Viewer::RequestFinish()
    {
        unique_lock<mutex> lock(mMutexFinish);
        mbFinishRequested = true;
    }

    bool Viewer::CheckFinish()
    {
        unique_lock<mutex> lock(mMutexFinish);
        return mbFinishRequested;
    }

    void Viewer::SetFinish()
    {
        unique_lock<mutex> lock(mMutexFinish);
        mbFinished = true;
    }

    bool Viewer::isFinished()
    {
        unique_lock<mutex> lock(mMutexFinish);
        return mbFinished;
    }

    void Viewer::RequestStop()
    {
        unique_lock<mutex> lock(mMutexStop);
        if(!mbStopped)
            mbStopRequested = true;
    }

    bool Viewer::isStopped()
    {
        unique_lock<mutex> lock(mMutexStop);
        return mbStopped;
    }

    bool Viewer::Stop()
    {
        unique_lock<mutex> lock(mMutexStop);
        unique_lock<mutex> lock2(mMutexFinish);

        if(mbFinishRequested)
            return false;
        else if(mbStopRequested)
        {
            mbStopped = true;
            mbStopRequested = false;
            return true;
        }

        return false;

    }

    void Viewer::Release()
    {
        unique_lock<mutex> lock(mMutexStop);
        mbStopped = false;
    }

}
