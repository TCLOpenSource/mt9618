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
#include "cxdref_demod.h"
#include "cxdref_integ.h"

cxdref_result_t cxdref_integ_Create (cxdref_integ_t * pInteg,
                                 cxdref_demod_t * pDemod,
                                 cxdref_demod_create_param_t * pCreateParam,
                                 cxdref_i2c_t * pDemodI2c,
                                 cxdref_tuner_t * pTuner
#ifdef CXDREF_DEMOD_SUPPORT_SAT_DEVICE_CTRL
                                 ,cxdref_lnbc_t * pLnbc
#endif
                                 )
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_integ_Create");

    if ((!pInteg) || (!pDemod) || (!pCreateParam) || (!pDemodI2c)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_Create (pDemod, pCreateParam, pDemodI2c);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    pInteg->pDemod = pDemod;
    pInteg->pTuner = pTuner;

#ifdef CXDREF_DEMOD_SUPPORT_SAT_DEVICE_CTRL
    pInteg->pLnbc = pLnbc;
#endif

    cxdref_integ_ClearCancel (pInteg);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_Initialize (cxdref_integ_t * pInteg)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_integ_Initialize");

    if ((!pInteg) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_Initialize (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if ((pInteg->pTuner) && (pInteg->pTuner->Initialize)) {
        result = pInteg->pTuner->Initialize (pInteg->pTuner);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x00);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

#ifdef CXDREF_DEMOD_SUPPORT_SAT_DEVICE_CTRL
    if ((pInteg->pLnbc) && (pInteg->pLnbc->Initialize)) {
        result = pInteg->pLnbc->Initialize (pInteg->pLnbc);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if ((pInteg->pLnbc) && (pInteg->pLnbc->Sleep)) {
        result = pInteg->pLnbc->Sleep (pInteg->pLnbc);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }
#endif

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_Sleep (cxdref_integ_t * pInteg)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_integ_Sleep");

    if ((!pInteg) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) &&
        (pInteg->pDemod->state != CXDREF_DEMOD_STATE_SHUTDOWN)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_Sleep (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if ((pInteg->pTuner) && (pInteg->pTuner->Sleep)) {
        result = pInteg->pTuner->Sleep (pInteg->pTuner);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x00);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_Shutdown (cxdref_integ_t * pInteg)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_integ_Shutdown");

    if ((!pInteg) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) &&
        (pInteg->pDemod->state != CXDREF_DEMOD_STATE_SHUTDOWN)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pInteg->pDemod->state == CXDREF_DEMOD_STATE_SHUTDOWN) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    result = cxdref_demod_Sleep (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if ((pInteg->pTuner) && (pInteg->pTuner->Shutdown)) {
        result = pInteg->pTuner->Shutdown (pInteg->pTuner);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x00);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

#ifdef CXDREF_DEMOD_SUPPORT_SAT_DEVICE_CTRL
    if ((pInteg->pLnbc) && (pInteg->pLnbc->Sleep)) {
        result = pInteg->pLnbc->Sleep (pInteg->pLnbc);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }
#endif

    result = cxdref_demod_Shutdown (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_Cancel (cxdref_integ_t * pInteg)
{
    CXDREF_TRACE_ENTER ("cxdref_integ_Cancel");

    if (!pInteg) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    cxdref_atomic_set (&(pInteg->cancel), 1);

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_integ_ClearCancel (cxdref_integ_t * pInteg)
{
    CXDREF_TRACE_ENTER ("cxdref_integ_ClearCancel");

    if (!pInteg) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    cxdref_atomic_set (&(pInteg->cancel), 0);

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_integ_CheckCancellation (cxdref_integ_t * pInteg)
{
    CXDREF_TRACE_ENTER ("cxdref_integ_CheckCancellation");

    if (!pInteg) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (cxdref_atomic_read (&(pInteg->cancel)) != 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_CANCEL);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
