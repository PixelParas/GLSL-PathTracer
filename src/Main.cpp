/*
 * MIT License
 *
 * Copyright(c) 2019-2021 Asif Ali
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this softwareand associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 *
 * The above copyright noticeand this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _USE_MATH_DEFINES
#include <SDL2/SDL.h>
#include <GL/gl3w.h>

#include <time.h>
#include <math.h>
#include <string>

#include "Scene.h"
#include "TiledRenderer.h"
#include "Camera.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "Loader.h"
#include "boyTestScene.h"
#include "ajaxTestScene.h"
#include "cornellTestScene.h"
#include "ImGuizmo.h"
#include "tinydir.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

using namespace std;
using namespace GLSLPT;

Scene* scene = nullptr;
Renderer* renderer = nullptr;

std::vector<string> sceneFiles;

float mouseSensitivity = 0.01f;
bool keyPressed = false;
int sampleSceneIndex = 0;
int selectedInstance = 0;
double lastTime = SDL_GetTicks();
bool done = false;

std::string shadersDir = "../src/shaders/";
std::string assetsDir = "../assets/";

RenderOptions renderOptions;
ImVec2 viewportPanelSize;
bool viewportFocused = false;
bool viewportHovered = false;

uint32_t denoisedTex;
ImVec2 denoisedTexSize;

struct LoopData
{
    SDL_Window* mWindow = nullptr;
    SDL_GLContext mGLContext = nullptr;
};

void SetImGuiTheme()
{
    auto& colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg]               = ImVec4{ 0.031f, 0.031f, 0.031f,  1.0f }; 

    // Headers
    colors[ImGuiCol_Header]                 = ImVec4{ 0.219f, 0.219f,  0.219f, 1.0f };
    colors[ImGuiCol_HeaderHovered]          = ImVec4{ 0.180f, 0.180f,  0.180f, 1.0f };
    colors[ImGuiCol_HeaderActive]           = ImVec4{ 0.219f, 0.219f,  0.219f, 1.0f };
                                                              
    // Buttons                                                
    colors[ImGuiCol_Button]                 = ImVec4{ 0.219f, 0.219f,  0.219f, 1.0f }; 
    colors[ImGuiCol_ButtonHovered]          = ImVec4{ 0.180f, 0.180f,  0.180f, 1.0f }; 
    colors[ImGuiCol_ButtonActive]           = ImVec4{ 0.219f, 0.219f,  0.219f, 1.0f }; 
                                                              
    // Frame BG                                               
    colors[ImGuiCol_FrameBg]                = ImVec4{ 0.113f, 0.113f,  0.113f, 1.0f }; 
    colors[ImGuiCol_FrameBgHovered]         = ImVec4{ 0.141f, 0.141f,  0.141f, 1.0f }; 
    colors[ImGuiCol_FrameBgActive]          = ImVec4{ 0.113f, 0.113f,  0.113f, 1.0f }; 
                                                              
    // Tabs                                                   
    colors[ImGuiCol_Tab]                    = ImVec4{ 0.113f, 0.113f,  0.113f, 1.0f };
    colors[ImGuiCol_TabHovered]             = ImVec4{ 0.141f, 0.141f,  0.141f, 1.0f };
    colors[ImGuiCol_TabActive]              = ImVec4{ 0.113f, 0.113f,  0.113f, 1.0f };
    colors[ImGuiCol_TabUnfocused]           = ImVec4{ 0.141f, 0.141f,  0.141f, 1.0f };
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4{ 0.113f, 0.113f,  0.113f, 1.0f };

    // Title
    colors[ImGuiCol_TitleBg]                = ImVec4{ 0.15f,  0.1505f, 0.151f, 1.0f };
    colors[ImGuiCol_TitleBgActive]          = ImVec4{ 0.15f,  0.1505f, 0.151f, 1.0f };
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4{ 0.15f,  0.1505f, 0.151f, 1.0f };

    //Sliders
    colors[ImGuiCol_SliderGrab]             = ImVec4{ 0.537f, 0.160f,  1.0f,   1.0f };                
    colors[ImGuiCol_SliderGrabActive]       = ImVec4{ 0.172f, 0.172f,  0.172f, 1.0f };         
}

void GetSceneFiles()
{
    tinydir_dir dir;
    int i;
    tinydir_open_sorted(&dir, assetsDir.c_str());

    for (i = 0; i < dir.n_files; i++)
    {
        tinydir_file file;
        tinydir_readfile_n(&dir, &file, i);

        if (std::string(file.extension) == "scene")
        {
            sceneFiles.push_back(assetsDir + std::string(file.name));
        }
    }

    tinydir_close(&dir);
}

void LoadScene(std::string sceneName)
{
    delete scene;
    scene = new Scene();
    LoadSceneFromFile(sceneName, scene, renderOptions);
    selectedInstance = 0;
    scene->renderOptions = renderOptions;
}

bool InitRenderer()
{
    delete renderer;
    renderer = new TiledRenderer(scene, shadersDir);
    renderer->Init();
    return true;
}

void ImGuiDrawFloat3(const std::string& label, float* values, float defaultValue = 0.0f, float columnWidth = 100.0f)
{
    ImGuiIO& io = ImGui::GetIO();
    auto componentFont = io.Fonts->Fonts[0];

    ImGui::PushID(label.c_str());

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, columnWidth);
    ImGui::Text(label.c_str());
    ImGui::NextColumn();

    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

    float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
    ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushFont(componentFont);
    if (ImGui::Button("X", buttonSize))
        values[0] = defaultValue;
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::DragFloat("##X", &values[0], 0.01f);
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushFont(componentFont);
    if (ImGui::Button("Y", buttonSize))
        values[1] = defaultValue;
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::DragFloat("##Y", &values[1], 0.01f);
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushFont(componentFont);
    if (ImGui::Button("Z", buttonSize))
        values[2] = defaultValue;
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::DragFloat("##Z", &values[2], 0.01f);
    ImGui::PopItemWidth();

    ImGui::PopStyleVar();

    ImGui::Columns(1);

    ImGui::PopID();
}

void SaveFrame(const std::string filename)
{
    unsigned char* data = nullptr;
    int w, h;
    renderer->GetOutputBuffer(&data, w, h);
    stbi_flip_vertically_on_write(true);
    stbi_write_png(filename.c_str(), w, h, 3, data, w * 3);
    printf("Frame saved: %s\n", filename.c_str());
    delete data;
}

bool denoised = false;
void Render()
{
    auto io = ImGui::GetIO();
    renderer->Render();
    //Viewport Setup
    ImGui::Begin("Viewport");
    {
        viewportFocused = ImGui::IsWindowFocused();
        viewportHovered = ImGui::IsWindowHovered();
        viewportPanelSize = ImGui::GetContentRegionAvail();
        if (scene->renderOptions.resolution.x != viewportPanelSize.x || scene->renderOptions.resolution.y != viewportPanelSize.y)
        {
            scene->renderOptions.resolution.x = viewportPanelSize.x;
            scene->renderOptions.resolution.y = viewportPanelSize.y;
            scene->camera->isMoving = true;
            InitRenderer();
        }
        if (scene->camera->isMoving)
        {
            renderer->Present();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, viewportPanelSize.x, viewportPanelSize.y);
        uint32_t tex = renderer->SetViewport(10, 10);
        ImGui::Image((void*)tex, viewportPanelSize, ImVec2(0, 1), ImVec2(1, 0));

        ImGui::End();
    }
    ImGui::Render();
    ImGui::UpdatePlatformWindows();

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Update(float secondsElapsed)
{
    keyPressed = false;
    if (ImGui::IsAnyMouseDown())
    {
        ImVec2 mousePos = ImGui::GetMousePos();
        if (viewportHovered) //Chech if mouse is on viewport TODO(PIXEL): object mouse picking
        {
            if (ImGui::IsMouseDown(0))
            {
                //Rotate Mouse around center
                ImVec2 mouseDelta = ImGui::GetMouseDragDelta(0, 0);
                scene->camera->OffsetOrientation(mouseDelta.x, mouseDelta.y);
                ImGui::ResetMouseDragDelta(0);
            }
            else if (ImGui::IsMouseDown(1))
            {
                //Move mouse the in Z axis
                ImVec2 mouseDelta = ImGui::GetMouseDragDelta(1, 0);
                scene->camera->SetRadius(mouseSensitivity * mouseDelta.y);
                ImGui::ResetMouseDragDelta(1);
            }
            else if (ImGui::IsMouseDown(2))
            {
                //Move mouse the in the XY Plane
                ImVec2 mouseDelta = ImGui::GetMouseDragDelta(2, 0);
                scene->camera->Strafe(mouseSensitivity * mouseDelta.x, mouseSensitivity * mouseDelta.y);
                ImGui::ResetMouseDragDelta(2);
            }
            scene->camera->isMoving = true;
        }
    }

    renderer->Update(secondsElapsed);
}

//Gizmos Are TODO to make the suite the path tracer
void EditTransform(const float* view, const float* projection, float* matrix)
{
    static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
    static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);

    if (ImGui::IsKeyPressed(90))
    {
        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    }

    if (ImGui::IsKeyPressed(69))
    {
        mCurrentGizmoOperation = ImGuizmo::ROTATE;
    }

    if (ImGui::IsKeyPressed(82))
    {
        mCurrentGizmoOperation = ImGuizmo::SCALE;
    }

    if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
    {
        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    }

    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
    {
        mCurrentGizmoOperation = ImGuizmo::ROTATE;
    }

    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
    {
        mCurrentGizmoOperation = ImGuizmo::SCALE;
    }

    float matrixTranslation[3], matrixRotation[3], matrixScale[3];
    ImGuizmo::DecomposeMatrixToComponents(matrix, matrixTranslation, matrixRotation, matrixScale);
    //ImGui::DragFloat3("Tr", matrixTranslation, 0.1f);
    //ImGui::DragFloat3("Rt", matrixRotation   , 0.1f);
    //ImGui::DragFloat3("Sc", matrixScale      , 0.1f);

    ImGuiDrawFloat3("Position", matrixTranslation);
    ImGuiDrawFloat3("Rotation", matrixRotation);
    ImGuiDrawFloat3("Scale", matrixScale);

    ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix);

    /*if (mCurrentGizmoOperation != ImGuizmo::SCALE)
    {
        if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
        {
            mCurrentGizmoMode = ImGuizmo::LOCAL;
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
        {
            mCurrentGizmoMode = ImGuizmo::WORLD;
        }
    }*/

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::Manipulate(view, projection, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, NULL);
}

void MainLoop(void* arg)
{
    LoopData& loopdata = *(LoopData*)arg;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT)
        {
            done = true;
        }
        if (event.type == SDL_WINDOWEVENT)
        {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(loopdata.mWindow))
            {
                done = true;
            }
        }
    }


    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGuizmo::SetOrthographic(false);
    ImGui::DockSpaceOverViewport();
    ImGuizmo::BeginFrame();
    {
        ImGui::Begin("Settings");
        ImGui::Text("Samples: %d ", renderer->GetSampleCount());

        ImGui::BulletText("LMB + drag to rotate");
        ImGui::BulletText("MMB + drag to pan");
        ImGui::BulletText("RMB + drag to zoom in/out");

        if (ImGui::Button("Save Screenshot"))
        {
            SaveFrame("./img_" + to_string(renderer->GetSampleCount()) + ".png");
        }

        std::vector<const char*> scenes;
        for (int i = 0; i < sceneFiles.size(); ++i)
        {
            scenes.push_back(sceneFiles[i].c_str());
        }

        if (ImGui::Combo("Scene", &sampleSceneIndex, scenes.data(), scenes.size()))
        {
            LoadScene(sceneFiles[sampleSceneIndex]);
            InitRenderer();
        }

        bool optionsChanged = false;

        optionsChanged |= ImGui::SliderFloat("Mouse Sensitivity", &mouseSensitivity, 0.01f, 1.0f);

        scene->camera->isMoving = false;

        if (optionsChanged)
        {
            scene->renderOptions = renderOptions;
            scene->camera->isMoving = true;
        }
        ImGui::End();

        ImGui::Begin("Render Settings");
        {
            bool requiresReload = false;
            Vec3* bgCol = &renderOptions.bgColor;

            optionsChanged |= ImGui::SliderInt("Max Depth", &renderOptions.maxDepth, 1, 10);
            requiresReload |= ImGui::Checkbox("Enable EnvMap", &renderOptions.useEnvMap);
            optionsChanged |= ImGui::SliderFloat("HDR multiplier", &renderOptions.hdrMultiplier, 0.1f, 10.0f);
            requiresReload |= ImGui::Checkbox("Enable RR", &renderOptions.enableRR);
            requiresReload |= ImGui::SliderInt("RR Depth", &renderOptions.RRDepth, 1, 10);
            requiresReload |= ImGui::Checkbox("Enable Constant BG", &renderOptions.useConstantBg);
            optionsChanged |= ImGui::ColorEdit3("Background Color", (float*)bgCol, 0);
            ImGui::Checkbox("Enable Denoiser", &renderOptions.enableDenoiser);

            if (requiresReload)
            {
                scene->renderOptions = renderOptions;
                InitRenderer();
            }
            if (ImGui::Button("Denoise"))
            {
                denoisedTex = renderer->Denoise();
                denoised = true;
                denoisedTexSize = viewportPanelSize;
            }

            if (ImGui::CollapsingHeader("Camera"))
            {
                float fov = Math::Degrees(scene->camera->fov);
                float aperture = scene->camera->aperture * 1000.0f;
                optionsChanged |= ImGui::SliderFloat("Fov", &fov, 10, 90);
                scene->camera->SetFov(fov);
                optionsChanged |= ImGui::SliderFloat("Aperture", &aperture, 0.0f, 10.8f);
                scene->camera->aperture = aperture / 1000.0f;
                optionsChanged |= ImGui::SliderFloat("Focal Distance", &scene->camera->focalDist, 0.01f, 50.0f);

                ImGuiIO& io = ImGui::GetIO();
                auto componentFont = io.Fonts->Fonts[0];

                ImGui::PushID("Position");

                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, 100.0f);
                ImGui::Text("Position");
                ImGui::NextColumn();

                ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

                float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
                ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
                ImGui::PushFont(componentFont);
                ImGui::Button("X", buttonSize);
                ImGui::PopFont();
                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                ImGui::Text("%f", scene->camera->position.x);
                ImGui::PopItemWidth();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
                ImGui::PushFont(componentFont);
                ImGui::Button("Y", buttonSize);
                ImGui::PopFont();
                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                ImGui::Text("%f", scene->camera->position.y);
                ImGui::PopItemWidth();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
                ImGui::PushFont(componentFont);
                ImGui::Button("Z", buttonSize);
                ImGui::PopFont();
                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                ImGui::Text("%f", scene->camera->position.z);
                ImGui::PopItemWidth();

                ImGui::PopStyleVar();

                ImGui::Columns(1);

                ImGui::PopID();


             //   ImGui::Text("Pos: %.2f, %.2f, %.2f", , scene->camera->position.y, scene->camera->position.z);
            }

            ImGui::End();
        }

        ImGui::Begin("Objects");
        {
            bool objectPropChanged = false;

            std::vector<std::string> listboxItems;
            for (int i = 0; i < scene->meshInstances.size(); i++)
            {
                listboxItems.push_back(scene->meshInstances[i].name);
            }

            // Object Selection
            if (ImGui::BeginListBox("Instances"))
            {
                for (int i = 0; i < scene->meshInstances.size(); i++)
                {
                    bool is_selected = selectedInstance == i;
                    if (ImGui::Selectable(listboxItems[i].c_str(), is_selected))
                    {
                        selectedInstance = i;
                    }
                }
                ImGui::EndListBox();
            }

            ImGui::Separator();

            // Transforms Properties
            ImGui::CollapsingHeader("Transforms");
            {
                float viewMatrix[16];
                float projMatrix[16];

                auto io = ImGui::GetIO();
                scene->camera->ComputeViewProjectionMatrix(viewMatrix, projMatrix, io.DisplaySize.x / io.DisplaySize.y);
                Mat4 xform = scene->meshInstances[selectedInstance].transform;

                EditTransform(viewMatrix, projMatrix, (float*)&xform);

                if (memcmp(&xform, &scene->meshInstances[selectedInstance].transform, sizeof(float) * 16))
                {
                    scene->meshInstances[selectedInstance].transform = xform;
                    objectPropChanged = true;
                }
            }

            ImGui::Separator();
            if(ImGui::CollapsingHeader("Materials"))
            {

                // Material Properties
                Vec3* albedo = &scene->materials[scene->meshInstances[selectedInstance].materialID].albedo;
                Vec3* emission = &scene->materials[scene->meshInstances[selectedInstance].materialID].emission;
                Vec3* extinction = &scene->materials[scene->meshInstances[selectedInstance].materialID].extinction;

                objectPropChanged |= ImGui::ColorEdit3("Albedo", (float*)albedo, 0);
                objectPropChanged |= ImGui::SliderFloat("Metallic", &scene->materials[scene->meshInstances[selectedInstance].materialID].metallic, 0.0f, 1.0f);
                objectPropChanged |= ImGui::SliderFloat("Roughness", &scene->materials[scene->meshInstances[selectedInstance].materialID].roughness, 0.001f, 1.0f);
                objectPropChanged |= ImGui::SliderFloat("Specular", &scene->materials[scene->meshInstances[selectedInstance].materialID].specular, 0.0f, 1.0f);
                objectPropChanged |= ImGui::SliderFloat("SpecularTint", &scene->materials[scene->meshInstances[selectedInstance].materialID].specularTint, 0.0f, 1.0f);
                objectPropChanged |= ImGui::SliderFloat("Subsurface", &scene->materials[scene->meshInstances[selectedInstance].materialID].subsurface, 0.0f, 1.0f);
                //objectPropChanged |= ImGui::SliderFloat("Anisotropic", &scene->materials[scene->meshInstances[selectedInstance].materialID].anisotropic, 0.0f, 1.0f);
                objectPropChanged |= ImGui::SliderFloat("Sheen", &scene->materials[scene->meshInstances[selectedInstance].materialID].sheen, 0.0f, 1.0f);
                objectPropChanged |= ImGui::SliderFloat("SheenTint", &scene->materials[scene->meshInstances[selectedInstance].materialID].sheenTint, 0.0f, 1.0f);
                objectPropChanged |= ImGui::SliderFloat("Clearcoat", &scene->materials[scene->meshInstances[selectedInstance].materialID].clearcoat, 0.0f, 1.0f);
                objectPropChanged |= ImGui::SliderFloat("clearcoatGloss", &scene->materials[scene->meshInstances[selectedInstance].materialID].clearcoatGloss, 0.0f, 1.0f);
                objectPropChanged |= ImGui::SliderFloat("Transmission", &scene->materials[scene->meshInstances[selectedInstance].materialID].transmission, 0.0f, 1.0f);
                objectPropChanged |= ImGui::SliderFloat("Ior", &scene->materials[scene->meshInstances[selectedInstance].materialID].ior, 1.001f, 2.0f);
                objectPropChanged |= ImGui::SliderFloat("atDistance", &scene->materials[scene->meshInstances[selectedInstance].materialID].atDistance, 0.05f, 10.0f);
                objectPropChanged |= ImGui::ColorEdit3("Extinction", (float*)extinction, 0);
            }
            if (objectPropChanged)
            {
                scene->RebuildInstances();
            }
            ImGui::End();
        }

        ImGui::Begin("Denoiser");
        {
            if (!denoised)
            {
                ImGui::Text("NO DENOISED IMAGE TO PREVIEW\nClick Denoise in Render TAB");
            }
            else
            {
                ImGui::Image((void*)denoisedTex, denoisedTexSize, ImVec2(0, 1), ImVec2(1, 0));
            }
            ImGui::End();
        }
    }

    double presentTime = SDL_GetTicks();
    Update((float)(presentTime - lastTime));
    lastTime = presentTime;
    glClearColor(0., 0., 0., 0.);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    Render();
    SDL_GL_SwapWindow(loopdata.mWindow);
}

int main(int argc, char** argv)
{
    srand((unsigned int)time(0));

    std::string sceneFile;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg(argv[i]);
        if (arg == "-s" || arg == "--scene")
        {
            sceneFile = argv[++i];
        }
        else if (arg[0] == '-')
        {
            printf("Unknown option %s \n'", arg.c_str());
            exit(0);
        }
    }

    if (!sceneFile.empty())
    {
        scene = new Scene();

        if (!LoadSceneFromFile(sceneFile, scene, renderOptions))
            exit(0);

        scene->renderOptions = renderOptions;
        std::cout << "Scene Loaded\n\n";
    }
    else
    {
        GetSceneFiles();
        LoadScene(sceneFiles[0]);
    }

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    LoopData loopdata;

#ifdef __APPLE__
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI /*| SDL_WINDOW_MAXIMIZED*/); //Commented start maximized for debugging
    loopdata.mWindow = SDL_CreateWindow("GLSL PathTracer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, renderOptions.resolution.x, renderOptions.resolution.y, window_flags);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

    //Create Context for glGenBuffer()
    loopdata.mGLContext = SDL_GL_CreateContext(loopdata.mWindow);
    gl3wInit();
    if (!loopdata.mGLContext)
    {
        fprintf(stderr, "Failed to initialize GL context!\n");
        return 1;
    }

    SDL_GL_SetSwapInterval(0); // Disable vsync

    // Initialize OpenGL loader
#if GL_VERSION_3_2
    bool err = false;
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    err = gladLoadGL() == 0;
#endif
    if (err)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }
#endif

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImGui_ImplSDL2_InitForOpenGL(loopdata.mWindow, loopdata.mGLContext);
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.Fonts->AddFontFromFileTTF("../assets/fonts/Roboto-Bold.ttf", 15.0f);
    io.FontDefault = io.Fonts->AddFontFromFileTTF("../assets/fonts/Roboto-Regular.ttf", 15.0f);

    //DarkMode
    ImGui::StyleColorsDark();
    SetImGuiTheme();

    if (!InitRenderer())
        return 1;
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags && ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    while (!done)
    {
        MainLoop(&loopdata);
    }

    delete renderer;
    delete scene;

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(loopdata.mGLContext);
    SDL_DestroyWindow(loopdata.mWindow);
    SDL_Quit();
    return 0;
}
