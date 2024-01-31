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

#include "cxdref_common.h"

#if defined(_WINDOWS)

#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

cxdref_result_t cxdref_stopwatch_start (cxdref_stopwatch_t * pStopwatch)
{
    CXDREF_TRACE_ENTER("cxdref_stopwatch_start");

    if (!pStopwatch) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }
    pStopwatch->startTime = timeGetTime ();

    CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_stopwatch_sleep (cxdref_stopwatch_t * pStopwatch, uint32_t ms)
{
    CXDREF_TRACE_ENTER("cxdref_stopwatch_sleep");
    if (!pStopwatch) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }
    CXDREF_ARG_UNUSED(*pStopwatch);
    CXDREF_SLEEP (ms);

    CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_stopwatch_elapsed (cxdref_stopwatch_t * pStopwatch, uint32_t* pElapsed)
{
    CXDREF_TRACE_ENTER("cxdref_stopwatch_elapsed");

    if (!pStopwatch || !pElapsed) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }
    *pElapsed = timeGetTime () - pStopwatch->startTime;

    CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
}

#elif defined(__linux__)

#include <linux/ktime.h>

static uint32_t GetTimeCount (void)
{
    struct timespec64 tp;

    ktime_get_ts64(&tp);

    return (uint32_t) ((tp.tv_sec * 1000) + (tp.tv_nsec / 1000000));
}

cxdref_result_t cxdref_stopwatch_start (cxdref_stopwatch_t * pStopwatch)
{
    CXDREF_TRACE_ENTER("cxdref_stopwatch_start");

    if (!pStopwatch) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    pStopwatch->startTime = GetTimeCount ();

    CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_stopwatch_sleep (cxdref_stopwatch_t * pStopwatch, uint32_t ms)
{
    CXDREF_TRACE_ENTER("cxdref_stopwatch_sleep");
    if (!pStopwatch) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }
    CXDREF_ARG_UNUSED(*pStopwatch);
    CXDREF_SLEEP (ms);

    CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_stopwatch_elapsed (cxdref_stopwatch_t * pStopwatch, uint32_t* pElapsed)
{
    CXDREF_TRACE_ENTER("cxdref_stopwatch_elapsed");

    if (!pStopwatch || !pElapsed) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_ARG);
    }
    *pElapsed = GetTimeCount () - pStopwatch->startTime;

    CXDREF_TRACE_RETURN(CXDREF_RESULT_OK);
}

#else

cxdref_result_t cxdref_stopwatch_start (cxdref_stopwatch_t * pStopwatch)
{
#error cxdref_stopwatch_start is not implemented
}

cxdref_result_t cxdref_stopwatch_sleep (cxdref_stopwatch_t * pStopwatch, uint32_t ms)
{
#error cxdref_stopwatch_sleep is not implemented
}

cxdref_result_t cxdref_stopwatch_elapsed (cxdref_stopwatch_t * pStopwatch, uint32_t* pElapsed)
{
#error cxdref_stopwatch_elapsed is not implemented
    return 0;
}

#endif
