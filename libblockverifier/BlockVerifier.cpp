/*
    This file is part of FISCO-BCOS.

    FISCO-BCOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    FISCO-BCOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file BlockVerifier.cpp
 *  @author mingzhenliu
 *  @date 20180921
 */
#include "BlockVerifier.h"
#include "ExecutiveContext.h"
#include <libethcore/PrecompiledContract.h>
#include <libethcore/TransactionReceipt.h>
#include <libexecutivecontext/ExecutionResult.h>
#include <libexecutivecontext/Executive.h>
#include <exception>
using namespace dev;
using namespace std;
using namespace dev::eth;
using namespace dev::blockverifier;

ExecutiveContext::Ptr BlockVerifier::executeBlock(Block& block)
{
    LOG(TRACE) << "BlockVerifier::executeBlock tx_num=" << block.transactions().size();
    ExecutiveContext::Ptr executiveContext = std::make_shared<ExecutiveContext>();
    try
    {
        BlockInfo blockInfo;
        blockInfo.hash = block.blockHeader().hash();
        blockInfo.number = block.blockHeader().number();
        executiveContext->setPrecompiledContract(m_precompiledContract);
        m_executiveContextFactory->initExecutiveContext(blockInfo, executiveContext);
    }
    catch (exception& e)
    {
        LOG(ERROR) << "Error:" << e.what();
    }
    unsigned i = 0;
    for (Transaction const& tr : block.transactions())
    {
        try
        {
            EnvInfo envInfo(block.blockHeader(), m_pNumberHash,
                block.getTransactionReceipts().back().gasUsed());
            envInfo.setPrecompiledEngine(executiveContext);
            std::pair<ExecutionResult, TransactionReceipt> resultReceipt =
                execute(envInfo, tr, OnOpFunc(), executiveContext);
            block.appendTransactionReceipt(resultReceipt.second);
        }
        catch (Exception& ex)
        {
            ex << errinfo_transactionIndex(i);
            throw;
        }
    }
    return executiveContext;
}


std::pair<ExecutionResult, TransactionReceipt> BlockVerifier::execute(EnvInfo const& _envInfo,
    Transaction const& _t, OnOpFunc const& _onOp, ExecutiveContext::Ptr executiveContext)
{
    LOG(TRACE) << "State::execute ";

    auto onOp = _onOp;
#if ETH_VMTRACE
    if (isChannelVisible<VMTraceChannel>())
        onOp = Executive::simpleTrace();  // override tracer
#endif


    // Create and initialize the executive. This will throw fairly cheaply and quickly if the
    // transaction is bad in any way.
    Executive e(*(executiveContext->getState()), _envInfo);
    ExecutionResult res;
    e.setResultRecipient(res);
    e.initialize(_t);

    // OK - transaction looks valid - execute.
    u256 startGasUsed = _envInfo.gasUsed();
    if (!e.execute())
        e.go(onOp);
    e.finalize();

    return make_pair(res,
        TransactionReceipt(executiveContext->getState()->rootHash(), startGasUsed + e.gasUsed(),
            e.logs(), e.status(), e.output().takeBytes(), e.newAddress()));
}