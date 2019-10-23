#ifndef _LIGHT_INC_
#define _LIGHT_INC_
// PBR Constants -----------------------------------------------

// The fresnel value for non-metals (dielectrics)
// Page 9: "F0 of nonmetals is now a constant 0.04"
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// Also slide 65 of http://blog.selfshadow.com/publications/s2014-shading-course/hoffman/s2014_pbs_physics_math_slides.pdf
const float F0_NON_METAL = 0.04f;

// Need a minimum roughness for when spec distribution function denominator goes to zero
const float MIN_ROUGHNESS = 0.0000001f; // 6 zeros after decimal

const float PI = 3.14159265359f;

#endif  // _LIGHT_INC_