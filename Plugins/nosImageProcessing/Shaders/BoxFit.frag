// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#version 450

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

layout(binding = 0) uniform sampler2D Input;
layout(binding = 1) uniform BoxFitParams
{
    ivec2 Resolution;
    vec4 BackgroundColor;
    uint FitMode;  // 0: Contain, 1: Cover
} Params;

void main()
{
    // Get content size from input texture
    ivec2 ContentSize = textureSize(Input, 0);
    
    // Calculate aspect ratios
    float ContainerAspect = float(Params.Resolution.x) / float(Params.Resolution.y);
    float ContentAspect = float(ContentSize.x) / float(ContentSize.y);
    
    // Calculate scaling factors
    vec2 Scale;
    if (Params.FitMode == 0) { // Contain
        if (ContainerAspect > ContentAspect) {
            Scale = vec2(ContentAspect / ContainerAspect, 1.0);
        } else {
            Scale = vec2(1.0, ContainerAspect / ContentAspect);
        }
    } else { // Cover
        if (ContainerAspect > ContentAspect) {
            Scale = vec2(1.0, ContainerAspect / ContentAspect);
        }
        else {
            Scale = vec2(ContentAspect / ContainerAspect, 1.0);
        }
    }
    
    // Transform UV coordinates
    vec2 ScaledUV = (uv - 0.5) / Scale + 0.5;
    
    // Check if the pixel is within the content bounds
    if (ScaledUV.x >= 0.0 && ScaledUV.x <= 1.0 && 
        ScaledUV.y >= 0.0 && ScaledUV.y <= 1.0) {
        rt = texture(Input, ScaledUV);
    } else {
        rt = Params.BackgroundColor;
    }
}
