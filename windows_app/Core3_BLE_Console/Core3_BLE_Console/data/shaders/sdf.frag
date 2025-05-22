#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

// NOTE: Add your custom variables here

void main()
{
    // Texel color fetching from texture sampler
    // NOTE: Calculate alpha using signed distance field (SDF)
    vec4 sdf_clr = texture(texture0, fragTexCoord);
    float distanceFromOutline = sdf_clr.a - 0.55;
    float distanceChangePerFragment = length(vec2(dFdx(distanceFromOutline), dFdy(distanceFromOutline)));
    float alpha = smoothstep(-distanceChangePerFragment, distanceChangePerFragment, distanceFromOutline);

    // Calculate final fragment color
    finalColor = vec4(fragColor.rgb, fragColor.a * alpha);
    //finalColor = vec4(sdf_clr.a, 0, 0, 1);
}
