module sh;
import modules.base;

/**
 * Represents a 3-band spherical harmonics
 */
public struct SH3 {
    private float coeff[9];

    public __init() {
        for(int i = 0; i < 9; i++) {
            coeff[i] = 0;
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
}