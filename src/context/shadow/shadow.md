# shadow

## Shadow Map Artifacts

- Perspective : mapping of pixels in view space to texels in the shadow map is not a one-to-one ratio. 

    `Perspective shadow maps (PSMs)` light space perspective shadow maps (`LSPSMs`) attempt to address perspective aliasing by skewing the light's projection matrix in order to place more texels near the eye where they are needed.
    Logarithmic perspective shadow maps (`LogPSMs`) also render a separate map per face of the view frustum. This technique uses nonlinear rasterization to place more texels near the eye. D3D10 and D3D11-class hardware do not support nonlinear rasterization. For more information about these techniques and algorithms, see the References section.

    Cascaded shadow maps ( `CSMs`) are the most popular technique for dealing with perspective aliasing. 

- Projective :  mapping between texels in camera space to texels in light space is not a one-to-one ratio; This is because of the orientation of the geometry with respect to the light camera.
- Shadow Acne and Erroneous Self-Shadowing
- Peter Panning

Techniques to Improve Shadow Maps:

- Slope-Scale Depth Bias
- Calculating a Tight Projection
- Moving the Light in Texel-Sized Increments