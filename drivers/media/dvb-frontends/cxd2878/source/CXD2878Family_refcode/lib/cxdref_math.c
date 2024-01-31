/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 */

#include "cxdref_math.h"

#define MAX_BIT_PRECISION        5
#define FRAC_BITMASK            0x1F
#define LOG2_10_100X            332
#define LOG2_E_100X             144

static const uint8_t log2LookUp[] = {
     0, 4,
     9,13,
    17,21,
    25,29,
    32,36,
    39,43,
    46,49,
    52,55,
    58,61,
    64,67,
    70,73,
    75,78,
    81,83,
    86,88,
    91,93,
    95,98
};

uint32_t cxdref_math_log2 (uint32_t x)
{
    uint8_t count = 0;
    uint8_t index = 0;
    uint32_t xval = x;

    for (x >>= 1; x > 0; x >>= 1) {
        count++;
    }

    x = count * 100;

    if (count > 0) {
        if (count <= MAX_BIT_PRECISION) {
            index = (uint8_t) (xval << (MAX_BIT_PRECISION - count)) & FRAC_BITMASK;
            x += log2LookUp[index];
        }
        else {
            index = (uint8_t) (xval >> (count - MAX_BIT_PRECISION)) & FRAC_BITMASK;
            x += log2LookUp[index];
        }
    }

    return (x);
}

uint32_t cxdref_math_log10 (uint32_t x)
{
    return ((100 * cxdref_math_log2 (x) + LOG2_10_100X / 2) / LOG2_10_100X);
}

uint32_t cxdref_math_log (uint32_t x)
{
    return ((100 * cxdref_math_log2 (x) + LOG2_E_100X / 2) / LOG2_E_100X);
}
