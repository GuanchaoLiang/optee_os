/*
 * Copyright (c) 2016, Hisilicon Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <types_ext.h>
#include <utee_types.h>
#include <kernel/tee_ta_manager.h>
#include <kernel/msg_param.h>
#include <mm/tee_mmu.h>
#include <mm/core_memprot.h>
#include <mm/mobj.h>
#include <tee/tee_svc.h>
#include <string.h>

#include <svc_agent.h>

static TEE_Result tee_agent_call_internal(unsigned int agent_id, void *va,
					  size_t len, bool user)
{
	TEE_Result res;
	struct optee_msg_param params[2];
	struct mobj *mobj;
	uint64_t cookie = 0;
	void *kva;

	/* va and len must be both null or be both valid */
	if ((!va && len)||(va && !len))
		return TEE_ERROR_BAD_PARAMETERS;

	if (va && len) {
		mobj = thread_rpc_alloc_kern_payload(len, &cookie);
		if (!mobj)
			return TEE_ERROR_OUT_OF_MEMORY;

		kva = mobj_get_va(mobj, 0);
		if (!kva) {
			res = TEE_ERROR_GENERIC;
			goto out;
		}
		if (user) {
			res = tee_svc_copy_from_user(kva, va, len);
			if (res != TEE_SUCCESS)
				goto out;
		} else {
			memcpy(kva, va, len);
		}
	}
	memset(params, 0, sizeof(params));
	params[0].attr = OPTEE_MSG_ATTR_TYPE_VALUE_INPUT;
	params[0].u.value.a = agent_id;

	if (!msg_param_init_memparam(params + 1, mobj, 0, len, cookie,
				     MSG_PARAM_MEM_DIR_INOUT)) {
		res = TEE_ERROR_BAD_STATE;
		goto out;
	}

	res = thread_rpc_cmd(OPTEE_MSG_RPC_CMD_AGENT, 2, params);
	if (res != TEE_SUCCESS)
		goto out;

	if (va && len) {
		if (user)
			res = tee_svc_copy_to_user(va, kva, len);
		else
			memcpy(va, kva, len);
	}
out:
	if (cookie)
		thread_rpc_free_kern_payload(cookie, mobj);

	return res;
}

#ifdef CFG_WITH_USER_TA
TEE_Result syscall_agent_call(unsigned int agent_id, void *uva, size_t len)
{
	return tee_agent_call_internal(agent_id, uva, len, true);
}
#endif

TEE_Result tee_agent_call(unsigned int agent_id, void *kva, size_t len)
{
	return tee_agent_call_internal(agent_id, kva, len, false);
}


