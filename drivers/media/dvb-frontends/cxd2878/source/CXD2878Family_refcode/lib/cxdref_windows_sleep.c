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

#include "cxdref_windows_sleep.h"
#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

int cxdref_windows_Sleep (unsigned long sleepTimeMs)
{
    BOOL qpcSupported = TRUE;
    LARGE_INTEGER qpcStartTime;
    LARGE_INTEGER qpcFrequency;

    if (sleepTimeMs == 0) {
        return 1;
    }

    if (!QueryPerformanceFrequency(&qpcFrequency)) {
        qpcSupported = FALSE;
    }

    if (!QueryPerformanceCounter(&qpcStartTime)) {
        qpcSupported = FALSE;
    }

    Sleep(sleepTimeMs);

    if (qpcSupported) {
        LARGE_INTEGER qpcCurrentTime;
        unsigned long currentDelayMs = 0;
        unsigned long targetDelayMs = sleepTimeMs;

        do {
            if (!QueryPerformanceCounter(&qpcCurrentTime)) {
                return 0;
            }

            currentDelayMs = (unsigned long)(1000 * (qpcCurrentTime.QuadPart - qpcStartTime.QuadPart) / qpcFrequency.QuadPart);

            if (currentDelayMs >= targetDelayMs) {
                break;
            }
        } while(1);
    }

    return 1;
}

int cxdref_windows_ImproveTimeResolution (void)
{
    MMRESULT mmResult = 0;
    TIMECAPS timeCaps;

    mmResult = timeGetDevCaps(&timeCaps, sizeof(timeCaps));
    if (mmResult != TIMERR_NOERROR) {
        return 0;
    }

    mmResult = timeBeginPeriod(timeCaps.wPeriodMin);
    if (mmResult != TIMERR_NOERROR) {
        return 0;
    }

    return 1;
}

int cxdref_windows_RestoreTimeResolution (void)
{
    MMRESULT mmResult = 0;
    TIMECAPS timeCaps;

    mmResult = timeGetDevCaps(&timeCaps, sizeof(timeCaps));
    if (mmResult != TIMERR_NOERROR) {
        return 0;
    }

    mmResult = timeEndPeriod(timeCaps.wPeriodMin);
    if (mmResult != TIMERR_NOERROR) {
        return 0;
    }

    return 1;
}
