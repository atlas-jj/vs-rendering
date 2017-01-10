//
// Created by shida on 06/12/16.
//

#include "Modeler/ModelDrawer.h"
#include "MapDrawer.h"
#include "MapPoint.h"
#include "KeyFrame.h"
#include <pangolin/pangolin.h>
#include <mutex>

namespace ORB_SLAM2
{

    ModelDrawer::ModelDrawer():mbModelUpdateRequested(false), mbModelUpdateDone(true)
    {
    }

    void ModelDrawer::DrawModel()
    {
        // select 4 KFs
        int numKFs = 1;
        vector<pair<cv::Mat,TextureFrame>> imAndTexFrame = mpModeler->GetTextures(numKFs);

        if (imAndTexFrame.size() >= numKFs) {
//            static unsigned int frameTex[4] = {0, 0, 0, 0};
            static unsigned int frameTex[1] = {0};
            if (!frameTex[0])
                glGenTextures(numKFs, frameTex);

            cv::Size imSize = imAndTexFrame[0].first.size();

            for (int i = 0; i < numKFs; i++) {
                glBindTexture(GL_TEXTURE_2D, frameTex[i]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
                if (imAndTexFrame[i].first.channels() == 1)
                {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                                 imSize.width, imSize.height, 0,
                                 GL_RED,
                                 GL_UNSIGNED_BYTE,
                                 imAndTexFrame[i].first.data);
                    GLint swizzleMask[] = {GL_RED, GL_RED, GL_RED, GL_RED};
                    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
                }
                else if (imAndTexFrame[i].first.channels() == 3)
                {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                                 imSize.width, imSize.height, 0,
                                 GL_RGB,
                                 GL_UNSIGNED_BYTE,
                                 imAndTexFrame[i].first.data);
                }

            }

            UpdateModel();

            glEnable(GL_TEXTURE_2D);

            glBegin(GL_TRIANGLES);
            glColor3f(1.0,1.0,1.0);

            for (list<dlovi::Matrix>::const_iterator it = GetTris().begin(); it != GetTris().end(); it++) {

                dlovi::Matrix point0 = GetPoints()[(*it)(0)];
                dlovi::Matrix point1 = GetPoints()[(*it)(1)];
                dlovi::Matrix point2 = GetPoints()[(*it)(2)];

                dlovi::Matrix edge10 = point1 - point0;
                dlovi::Matrix edge20 = point2 - point0;

                dlovi::Matrix normal = edge20.cross(edge10);
                normal = normal / normal.norm();

                glNormal3d(normal(0), normal(1), normal(2));

                TextureFrame tex = imAndTexFrame[0].second;
                vector<float> uv0 = tex.GetTexCoordinate(point0(0),point0(1),point0(2),imSize);
                vector<float> uv1 = tex.GetTexCoordinate(point1(0),point1(1),point1(2),imSize);
                vector<float> uv2 = tex.GetTexCoordinate(point2(0),point2(1),point2(2),imSize);

                if (uv0[0] > 0 && uv0[0] < 1 && uv0[1] > 0 && uv0[1] < 1 &&
                    uv1[0] > 0 && uv1[0] < 1 && uv1[1] > 0 && uv1[1] < 1 &&
                    uv2[0] > 0 && uv2[0] < 1 && uv2[1] > 0 && uv2[1] < 1) {
                    glTexCoord2f(uv0[0], uv0[1]);
                    glVertex3d(point0(0), point0(1), point0(2));

                    glTexCoord2f(uv1[0], uv1[1]);
                    glVertex3d(point1(0), point1(1), point1(2));

                    glTexCoord2f(uv2[0], uv2[1]);
                    glVertex3d(point2(0), point2(1), point2(2));
                }
            }
            glEnd();

            glDisable(GL_TEXTURE_2D);
        }
    }

    void ModelDrawer::DrawModelPoints()
    {
        UpdateModel();

        glPointSize(3);
        glBegin(GL_POINTS);
        glColor3f(0.5, 0.5, 0.5);
        for (size_t i = 0; i < GetPoints().size(); i++) {
            glVertex3d(GetPoints()[i](0), GetPoints()[i](1), GetPoints()[i](2));
        }
        glEnd();
    }

    void ModelDrawer::DrawTriangles(pangolin::OpenGlMatrix &Twc)
    {
        UpdateModel();

        glPushMatrix();

#ifdef HAVE_GLES
        glMultMatrixf(Twc.m);
#else
        glMultMatrixd(Twc.m);
#endif

        GLfloat light_position[] = { 0.0, 0.0, 1.0, 0.0 };
////            GLfloat light_position[] = {10, 10, 10, 0};
        glLightfv(GL_LIGHT0, GL_POSITION, light_position);

        glPopMatrix();

        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);

        glShadeModel(GL_FLAT);

        GLfloat material_diffuse[] = {0.2, 0.5, 0.8, 1};
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, material_diffuse);

        glBegin(GL_TRIANGLES);
        glColor3f(1.0,1.0,1.0);

        for (list<dlovi::Matrix>::const_iterator it = GetTris().begin(); it != GetTris().end(); it++) {

            dlovi::Matrix point0 = GetPoints()[(*it)(0)];
            dlovi::Matrix point1 = GetPoints()[(*it)(1)];
            dlovi::Matrix point2 = GetPoints()[(*it)(2)];

            dlovi::Matrix edge10 = point1 - point0;
            dlovi::Matrix edge20 = point2 - point0;

            dlovi::Matrix normal = edge20.cross(edge10);
            normal = normal / normal.norm();

            glNormal3d(normal(0), normal(1), normal(2));

            glVertex3d(point0(0), point0(1), point0(2));
            glVertex3d(point1(0), point1(1), point1(2));
            glVertex3d(point2(0), point2(1), point2(2));

        }
        glEnd();

        glDisable(GL_LIGHTING);

    }

    void ModelDrawer::DrawFrame()
    {
        // select the last frame
        int numKFs = 1;
        vector<pair<cv::Mat,TextureFrame>> imAndTexFrame = mpModeler->GetTextures(numKFs);

        if (imAndTexFrame.size() >= numKFs) {
            glColor3f(1.0,1.0,1.0);

            cv::Size imSize = imAndTexFrame[0].first.size();
            pangolin::GlTexture imageTexture(imSize.width, imSize.height, GL_RGB, false, 0, GL_RGB, GL_UNSIGNED_BYTE);
            imageTexture.Upload(imAndTexFrame[0].first.data, GL_RGB, GL_UNSIGNED_BYTE);
            imageTexture.RenderToViewport(true);

        }
    }


    void ModelDrawer::UpdateModel()
    {
        if(mbModelUpdateRequested && ! mbModelUpdateDone)
            return;

        if(mbModelUpdateRequested && mbModelUpdateDone){
            mModel = mUpdatedModel;
            mbModelUpdateRequested = false;
            return;
        }

        mbModelUpdateDone = false;
        mbModelUpdateRequested = true; // implicitly signals SurfaceInferer thread which is polling
    }

    void ModelDrawer::SetUpdatedModel(const vector<dlovi::Matrix> & modelPoints, const list<dlovi::Matrix> & modelTris)
    {
        mUpdatedModel.first = modelPoints;
        mUpdatedModel.second = modelTris;
    }

    vector<dlovi::Matrix> & ModelDrawer::GetPoints()
    {
        return mModel.first;
    }

    list<dlovi::Matrix> & ModelDrawer::GetTris()
    {
        return mModel.second;
    }

    void ModelDrawer::MarkUpdateDone()
    {
        mbModelUpdateDone = true;
    }

    bool ModelDrawer::UpdateRequested()
    {
        return mbModelUpdateRequested;
    }

    bool ModelDrawer::UpdateDone()
    {
        return mbModelUpdateDone;
    }

    void ModelDrawer::SetModeler(Modeler* pModeler)
    {
        mpModeler = pModeler;
    }

} //namespace ORB_SLAM