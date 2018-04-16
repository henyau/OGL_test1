#version 330 core
out vec4 FragColor;
//PBR test
//pbr texture map is interpreted as [R,G,B]->[metallic, roughness, ao]
//
// BRDF implementation based off of https://github.com/JoeyDeVries/LearnOpenGL
//D) GGX Distribution
//F) Schlick-Fresnel
//V) Schlick approximation of Smith solved with GGX


const float PI = 3.14159265359;

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec3 TangentLightPos;
    vec3 TangentViewPos;
    vec3 TangentFragPos;
} fs_in;

uniform sampler2D diffuseMap;
uniform sampler2D emissiveMap;
uniform sampler2D normalMap;
uniform sampler2D pbrMap;
uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];
uniform vec3 camPos;

// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float alpha = roughness*roughness;
    float alphasrd = alpha*alpha;
    float NdotH = clamp(dot(N, H),0.0,2.0);
   
    float denom = (NdotH*NdotH * (alphasrd - 1.0) + 1.0);
    denom = PI * denom * denom;

    return alphasrd / max(denom, 0.0001);
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    float NdotL = clamp(dot(N, L), 0.0, 1.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main()
{
//interpret PBRmap values 
	float metallic = texture(pbrMap, fs_in.TexCoords).r;
	float roughness = texture(pbrMap, fs_in.TexCoords).g;
	float ao = texture(pbrMap, fs_in.TexCoords).b;

    vec3 V = normalize(camPos - fs_in.FragPos); //vec to eye

     // obtain normal from normal map in range [0,1]
    vec3 normal = texture(normalMap, fs_in.TexCoords).rgb;

	vec3 N = normal; //should be in world space or tangent space?
    // transform normal vector to range [-1,1]
    normal = normalize(normal * 2.0 - 1.0);  // this normal is in tangent space
    //treat diffuse as albedo
    vec3 color = texture(diffuseMap, fs_in.TexCoords).rgb;//
	//get emissive 
	vec3 emissive = texture(emissiveMap, fs_in.TexCoords).rgb;//
	
	//pbr stuff
	//calculate reflectance
	vec3 F0 = vec3(0.04); 
    F0 = mix(F0, color, metallic); //mix with how metallic

	// reflectance equation
    vec3 Lo = vec3(0.0);
    for(int i = 0; i < 4; ++i) 
    {
        // calculate per-light radiance
        vec3 L = normalize(lightPositions[i] - fs_in.FragPos);
        vec3 H = normalize(V + L);
        float distance = length(lightPositions[i] - fs_in.FragPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = lightColors[i] * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);
           
        vec3 nominator    = NDF * G * F; 
        float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        vec3 specular = nominator / max(denominator, 0.001); // prevent divide by zero for NdotV=0.0 or NdotL=0.0
        
        // kS is equal to Fresnel
        vec3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;	  

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);        

        // add to outgoing radiance Lo
        Lo += (kD * color / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    }   
    
	vec3 ambient = vec3(0.5) * color * ao+emissive; 
    vec3 colorMix = ambient + Lo;

    FragColor = vec4(colorMix, 1.0);
}