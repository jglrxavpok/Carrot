module sh;
import modules.base;

/**
 * Represents a 3-band spherical harmonics
 */
public struct SH3 {
    private float coeff[9];

    public __init() {
        reset(0.0f);
    }

    [mutating]
    public void reset(float defaultValue = 0.0f) {
        coeff[0] = defaultValue;
        for(int i = 1; i < 9; i++) {
            coeff[i] = 0;
        }
    }

    [mutating]
    public void integrate(float value, float3 direction) {
        for(int i = 0; i < 9; i++) {
            coeff[i] += value * sh(i, direction);
        }
    }

    // https://www.ppsloan.org/publications/StupidSH36.pdf, Stupid Spherical Harmonics (SH) Tricks
    private constexpr float sh(int coeffIndex, float3 direction) {
        switch(coeffIndex) {
            case 0:
                return 1.0f / (2 * sqrt(M_PIf));

            case 1: // L=1, M=-1
                return -sqrt(3.0f * M_INV_PIf) / 2 * direction.y;

            case 2: // L=1, M=0
                return sqrt(3.0f * M_INV_PIf) / 2 * direction.z;

            case 3: // L=1, M=1
                return -sqrt(3.0f * M_INV_PIf) / 2 * direction.x;

            case 4: // L=2, M=-2
                return sqrt(15.0f * M_INV_PIf) / 2 * direction.y * direction.x;

            case 5: // L=2, M=-1
                return -sqrt(15.0f * M_INV_PIf) / 2 * direction.y * direction.z;

            case 6: // L=2, M=0
                return sqrt(5.0f * M_INV_PIf) / 4 * (3 * direction.z * direction.z - 1);

            case 7: // L=2, M=1
                return -sqrt(15.0f * M_INV_PIf) / 2 * direction.x * direction.z;

            case 8: // L=2, M=2
                return sqrt(15.0f * M_INV_PIf) / 4 * (direction.x * direction.x - direction.y * direction.y);
        }
        return 0.0/0.0; // unreachable
    }

    /**
    * Sample the value represented by this spherical harmonics in the given direction (assumed normalized)
    */
    public float reconstruct(float3 direction) {
        float f = 0;
        for(int i = 0; i < 9; i++) {
            f += coeff[i] * sh(i, direction);
        }
        return f;
    }

    public float evaluateCosineLobe(float3 direction) {
        SH3 cosineLobeSH;
        SH_GetCoefficients_ClampedCosine(direction, cosineLobeSH.coeff);

        float f = 0;
        for(int i = 0; i < 9; i++) {
            f += cosineLobeSH.coeff[i] * coeff[i];
        }
        return f;
    }

    public float coeff0() {
        return coeff[0];
    }

    public SH3 add(const in SH3 b) {
        SH3 r = this;
        for(int i = 0; i < 9; i++) {
            r.coeff[i] += b.coeff[i];
        }
        return r;
    }

    public SH3 minus(const in SH3 b) {
        SH3 r = this;
        for(int i = 0; i < 9; i++) {
            r.coeff[i] -= b.coeff[i];
        }
        return r;
    }

    public SH3 scale(float f) {
        SH3 r = this;
        for(int i = 0; i < 9; i++) {
            r.coeff[i] *= f;
        }
        return r;
    }
}

public SH3 operator+=(out SH3 a, const in SH3 b) {
    a = a.add(b);
    return a;
}

public SH3 operator*=(out SH3 a, float f) {
    a = a.scale(f);
    return a;
}

public SH3 operator/=(out SH3 a, float f) {
    a = a.scale(1.0f / f);
    return a;
}


// Adapted from AMD's Capsaisin:
/**********************************************************************
Copyright (c) 2020 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/
void SH_GetCoefficients_ClampedCosine(in float3 cosine_lobe_dir, out float coefficients[9])
{
    // cosine_lobe_dir is normalized
    const float CosineA0 = M_PIf;
    const float CosineA1 = (2.0f * M_PIf) / 3.0f;
    const float CosineA2 = M_PIf / 4.0f;
    float fC0, fC1, fS0, fS1, fTmpA, fTmpB, fTmpC;
    float pz2 = cosine_lobe_dir.z * cosine_lobe_dir.z;
    coefficients[0] = 0.2820947917738781f * CosineA0;
    coefficients[2] = 0.4886025119029199f * cosine_lobe_dir.z * CosineA1;
    coefficients[6] = 0.7431238683011271f * pz2 + -0.2477079561003757f;
    fC0 = cosine_lobe_dir.x;
    fS0 = cosine_lobe_dir.y;
    fTmpA = -0.48860251190292f;
    coefficients[3] = fTmpA * fC0 * CosineA1;
    coefficients[1] = fTmpA * fS0 * CosineA1;
    fTmpB = -1.092548430592079f * cosine_lobe_dir.z;
    coefficients[7] = fTmpB * fC0 * CosineA2;
    coefficients[5] = fTmpB * fS0 * CosineA2;
    fC1 = cosine_lobe_dir.x * fC0 - cosine_lobe_dir.y * fS0;
    fS1 = cosine_lobe_dir.x * fS0 + cosine_lobe_dir.y * fC0;
    fTmpC = 0.5462742152960395f;
    coefficients[8] = fTmpC * fC1 * CosineA2;
    coefficients[4] = fTmpC * fS1 * CosineA2;
}