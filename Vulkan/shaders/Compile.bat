%VK_SDK_PATH%/Bin/glslc.exe VertexShader.vert -o vert.spv
%VK_SDK_PATH%/Bin/glslc.exe FragmentShader.frag -o frag.spv
%VK_SDK_PATH%/Bin/glslc.exe SkyboxVertexShader.vert -o skyboxVert.spv
%VK_SDK_PATH%/Bin/glslc.exe SkyboxFragmentShader.frag -o skyboxFrag.spv
%VK_SDK_PATH%/Bin/glslc.exe Reflective.frag -o reflectiveFrag.spv