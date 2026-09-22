//light space perspective shadow maps (LSPSMs)

/*
Perspective shadow maps(PSMs) and light space perspective shadow maps(LSPSMs) attempt to address perspective aliasing by skewing the light's projection matrix in order to place
 more texels near the eye where they are needed
   The parameterization of the transform required to map eye-space pixels to texels in the shadow map cannot be bound by a linear skew. A logarithmic parameterization is required.
*/
 