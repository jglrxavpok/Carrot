module debug;
import modules.base;

// Select what the merge-lighting shader will output
public enum MergeLightingOutput: uint {
    Default = 0, // Merged lighting with albedo, lighting, etc
    Albedo,
    Position,
    Depth,
    Normal,
    MetallicRoughness,
    Emissive,
    Randomness,
    Tangent,
    Lighting,
    Motion,
    Moments,
    EntityID,
    NoisyLighting,
    PostTemporalDenoise,
    Variance,
    PostFireflyRejection,

    VisibilityBufferFirst,
    VisibilityBufferTriangles = 17,
    VisibilityBufferClusters,
    VisibilityBufferLods,
    VisibilityBufferScreenError,
    VisibilityBufferLast = 20,

    // new values go here
}

public struct DebugParams {
    public MergeLightingOutput mergeLightingOutput;
}

// Adapted from: https://www.shadertoy.com/view/4s23DG
// 2D vector field visualization by Morgan McGuire, @morgan3d, http://casual-effects.com
//
// Copyright 2014, Morgan McGuire
// Provided under the MIT license https://opensource.org/license/mit/

public static const int   ARROW_V_STYLE = 1;
public static const int   ARROW_LINE_STYLE = 2;

// Choose your arrow head style
public static const int   ARROW_STYLE = ARROW_LINE_STYLE;
public static const float ARROW_TILE_SIZE = 32.0;

// How sharp should the arrow head be? Used
public static const float ARROW_HEAD_ANGLE = 45.0 * M_PIf / 180.0;

// Used for ARROW_LINE_STYLE
public static const float ARROW_HEAD_LENGTH = ARROW_TILE_SIZE / 6.0;
public static const float ARROW_SHAFT_THICKNESS = 3.0;

// v = field sampled at tileCenterCoord(p), scaled by the length
// desired in pixels for arrows
// Returns 1.0 where there is an arrow pixel.
public float arrow(float2 centerCoord, float2 p, float2 v) {
    // Make everything relative to the center, which may be fractional
	p -= centerCoord;
		
    float mag_v = length(v), mag_p = length(p);
	
	if (mag_v > 0.0) {
		// Non-zero velocity case
		vec2 dir_p = p / mag_p, dir_v = v / mag_v;
		
		// We can't draw arrows larger than the tile radius, so clamp magnitude.
		// Enforce a minimum length to help see direction
		mag_v = clamp(mag_v, 5.0, ARROW_TILE_SIZE / 2.0);

		// Arrow tip location
		v = dir_v * mag_v;
		
		// Define a 2D implicit surface so that the arrow is antialiased.
		// In each line, the left expression defines a shape and the right controls
		// how quickly it fades in or out.

		float dist;		
		if (ARROW_STYLE == ARROW_LINE_STYLE) {
			// Signed distance from a line segment based on https://www.shadertoy.com/view/ls2GWG by 
			// Matthias Reitinger, @mreitinger
			
			// Line arrow style
			dist = 
				max(
					// Shaft
					ARROW_SHAFT_THICKNESS / 4.0 - 
						max(abs(dot(p, vec2(dir_v.y, -dir_v.x))), // Width
						    abs(dot(p, dir_v)) - mag_v + ARROW_HEAD_LENGTH / 2.0), // Length
						
   			         // Arrow head
					 min(0.0, dot(v - p, dir_v) - cos(ARROW_HEAD_ANGLE / 2.0) * length(v - p)) * 2.0 + // Front sides
					 min(0.0, dot(p, dir_v) + ARROW_HEAD_LENGTH - mag_v)); // Back
		} else {
			// V arrow style
			dist = min(0.0, mag_v - mag_p) * 2.0 + // length
				   min(0.0, dot(normalize(v - p), dir_v) - cos(ARROW_HEAD_ANGLE / 2.0)) * 2.0 * length(v - p) + // head sides
				   min(0.0, dot(p, dir_v) + 1.0) + // head back
				   min(0.0, cos(ARROW_HEAD_ANGLE / 2.0) - dot(normalize(v * 0.33 - p), dir_v)) * mag_v * 0.8; // cutout
		}
		
		return clamp(1.0 + dist, 0.0, 1.0);
	} else {
		// Center of the pixel is always on the arrow
		return max(0.0, 1.2 - mag_p);
	}
}