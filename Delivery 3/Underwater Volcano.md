YouTube Link: https://youtu.be/YdCCQ9Ju8rI

![[Pasted image 20241213051956.png]]
#### Scene Setup
The scene is centered with an active volcano that keeps emitting smoke out of its crater. Vibrant lava flows at the volcano's base. Around the smoke, there is a group of fishes swimming in synchronized patterns. Each fish exhibits hierarchical animation, while interactive crabs on a nearby rock retreat when clicked.

#### Advanced Feature

##### Particle Effects
 class `ParticleSystem` is built to simulate volcanic smoke with thousands of particles having random velocities, lifetimes, texture mapping and alpha blending for realistic fading.
 - **Dynamic Adjustments:** Alpha and Particle Size
	```cpp
	float currentScale = mix(50, 150, pow(instanceLifetimePercent, 1.5));
	float alpha = pow(LifetimePercent, 0.5); // Fade out near the end
	```

- **Realistic Movement:** Particles rise with damping factors and random offsets, creating lifelike smoke behavior.
##### Alpha blending
The particle shader uses alpha blending for realistic particle fading and combined with red beam effects​​​.
  ```cpp
  	// Calculate direction to volcano
	vec3 toVolcano = normalize(FragPos - volcanoPosition);
	
	// Calculate alignment (cosine of angle)
	float alignment = dot(toVolcano, redLightDirection);
	
	// Beam effect using smoothstep
	float beamEffect = smoothstep(1.0 - beamWidth, 1.0, alignment);
	
	// Apply red light with beam effect
	float distanceToVolcano = length(FragPos - volcanoPosition);
	float falloff = exp(-distanceToVolcano * redLightFalloff);
	vec3 redLight = redLightColor * redLightIntensity * beamEffect * falloff;
	
	 // Combine particle color with red light
    FragColor = vec4((texColor.rgb * particleColor.rgb + redLight) * alpha, texColor.a * alpha);
	```
##### Procedural Terrain:
The lava plane is procedurally generated and animated using summation of sinusoidal waves for dynamic height adjustments​. The UV mapping of the lava is also procedural.
- Update heights for each vertex:
  ```cpp
float generateHeight(float x, float z, float time) 
{
    float frequency = 0.1f;  
    float amplitude = 2.0f;   
    float phaseShift = 0.3f;  
    // summation of two sinosoidal waves
    return amplitude * (sin(frequency * x + time) + cos(frequency * z + time * phaseShift));
}
```
- Update UV Mapping as time goes by for each vertex.
```cpp
// type: 3 means lava
vec2 texOffset = type !=3 ? vec2(0.0, 0.0) : vec2(timeInSeconds * 0.1, 
				timeInSeconds * 0.1);
TexCoords = tex_coords;
```

##### Intelligent Character Behavior:
Crabs exhibit responsive behavior by rotating themselves to face the user camera and moving away from when clicked, simulating retreat behavior​​. 

##### Hierarchical Animation with Instanced Rendering
Fish models have a hierarchical structure with independent animations for body, head, and fins. Furthermore, to generate a group of fish, instanced rendering is employed, which significantly optimizing performance by reducing draw calls. Each instance represents an individual part of fish with its own transformation.

```cpp
// Draw instanced fish animation:

// Calcualte the fish body rotation matrix
glm::mat4 rotateBodyMtx = glm::mat4(1.f);
rotateBodyMtx = glm::rotate(rotateBodyMtx, glm::radians(rotate_body), glm::vec3(0.0f, 1.0f, 0.0f));
vector<glm::mat4> TmpTransforms(FISHCOUNT, rotateBodyMtx);

// Calculate the transforms of all fish body instances
std::transform(fishInstanceTransforms.begin(), fishInstanceTransforms.end(), TmpTransforms.begin(),
	[&](const glm::mat4& transform) { return transform * rotateBodyMtx; });

// pass transforms to the shader
UpdateAnimationInstances(fishModel, TmpTransforms);

for (int i = 0; i < fishModel.mChildMeshes.size(); ++i)
{
	//  Calcualte the child rotation matrix
	auto& childMesh = fishModel.mChildMeshes[i];
	glm::mat4 rotateChildMtx(1.0);
	if (i == 0) // first child mesh is the head
	{
		rotateChildMtx = glm::rotate(rotateChildMtx, glm::radians(rotate_head), glm::vec3(0.0f, 1.0f, 0.0f));
	}
	else // second child mesh is the fin
	{
		rotateChildMtx = glm::rotate(rotateChildMtx, glm::radians(rotate_fin), glm::vec3(0.0f, 1.0f, 0.0f));
	}
	
	// Calculate the transforms of all child instances for the same part
	std::transform(fishInstanceTransforms.begin(), fishInstanceTransforms.end(), TmpTransforms.begin(),
		[&](const glm::mat4& transform) { return transform * rotateBodyMtx * fishModel.mChildMeshes[i].mLocalTransform * rotateChildMtx; });

	// pass transforms of child instances to the shader
	UpdateAnimationInstances(childMesh, TmpTransforms);
}
```

#### Lighting, Shading: 
##### Main Shader -- **Phong illumination**:
![[Pasted image 20241212022126.png]]
- Shader Implementation: The primary shader implements the Phong illumination, integrating ambient, diffuse, and specular components to render realistic lighting.
- Directional Light: Simulates sunlight penetrating the underwater environment with depth-based attenuation and procedural caustics for enhanced realism.
- Dynamic Lighting: The light oscillates over time, creating a lifelike interplay of light and shadow.
	1. Diffuse lighting with caustic
	```cpp
	float diff = max(dot(norm, lightDir), 0.2);
	float causticEffect = caustics(FragPos) * causticIntensity;
	vec3 diffuse = lightDiffuse * diff * causticEffect;
	```

2. Specular lighting
```cpp
vec3 reflectDir = reflect(-lightDir, norm);
float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64);
vec3 specular = vec3(0.8, 0.8, 1.0) * spec;
```

3. Attenuation with Depth
```cpp
float depth = abs(FragPos.y - 50);
float attenuation = exp(-depth * depthFalloff);
vec3 attenuatedLight = (ambient + diffuse + specular) * attenuation;
```

4. Combine all the lighting together:
```cpp
const vec3 shallowColor = vec3(0.0f, 0.4f, 0.8f); // Bright blue
const vec3 deepColor = vec3(0.0f, 0.1f, 0.3f);    // Dark blue
vec3 waterColor = mix(shallowColor, deepColor, depth / 100.0f);
vec3 finalLight = mix(waterColor, attenuatedLight, attenuation);
FragColor = vec4(finalLight * texColor.rgb, 1.0);
```

##### Particle Shader - spotlight & alpha blending:
- A red spotlight  is added to simulate the glow from volcano, blending dynamically with the smoke particles, which has beam effect and intensity falloff.
  ![[Pasted image 20241212020534.png|400]]
	```cpp
	vec3 redLight = redLightColor * redLightIntensity * beamEffect * falloff;
	
	// Combine particle color with red light
	FragColor = vec4((texColor.rgb * particleColor.rgb + redLight) * alpha, texColor.a * alpha);
	```

#### Texture Mapping
`TextureManager` class is implemented to manage all the loaded texture in the runtime, which can efficiently avoid the same texture being loaded multiple times which optimizes memory and runtime performance.  
- Textures are applied across all models, including procedural textures for dynamic lava surfaces. This class also supports various image formats​.
- **Procedural UV Mapping:** The lava plane demonstrates effective use of UV coordinates, enhancing visual detail.

#### Declaration

This work is all by myself.