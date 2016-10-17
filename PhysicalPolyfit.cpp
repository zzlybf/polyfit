/*
 *    _____      _ ____  ____
 *   / ___/_____(_) __ \/ __ )
 *   \__ \/ ___/ / / / / __  |
 *  ___/ / /__/ / /_/ / /_/ / 
 * /____/\___/_/_____/_____/  
 *
 *
 * BEGIN_COPYRIGHT
 *
 * This file is part of SciDB.
 * Copyright (C) 2008-2014 SciDB, Inc.
 *
 * SciDB is free software: you can redistribute it and/or modify
 * it under the terms of the AFFERO GNU General Public License as published by
 * the Free Software Foundation.
 *
 * SciDB is distributed "AS-IS" AND WITHOUT ANY WARRANTY OF ANY KIND,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,
 * NON-INFRINGEMENT, OR FITNESS FOR A PARTICULAR PURPOSE. See
 * the AFFERO GNU General Public License for the complete license terms.
 *
 * You should have received a copy of the AFFERO GNU General Public License
 * along with SciDB.  If not, see <http://www.gnu.org/licenses/agpl-3.0.html>
 *
 * END_COPYRIGHT
 */
#include "query/Operator.h"

namespace scidb
{

class PhysicalPolyfit : public PhysicalOperator
{
public:
    PhysicalPolyfit(string const& logicalName,
                string const& physicalName,
                Parameters const& parameters,
                ArrayDesc const& schema):
        PhysicalOperator(logicalName, physicalName, parameters, schema)
    {}

    virtual ArrayDistribution getOutputDistribution(vector<ArrayDistribution> const& inputDistributions,
                                                    vector<ArrayDesc> const& inputSchemas) const
    {
       return inputDistributions[0];
    }

    /**
      * [Optimizer API] Determine if operator changes result chunk distribution.
      * @param sourceSchemas shapes of all arrays that will given as inputs.
      * @return true if will changes output chunk distribution, false if otherwise
      */
    virtual bool changesDistribution(std::vector<ArrayDesc> const& sourceSchemas) const
    {
        return false;
    }

    /* The instance-parallel 'main' routine of this operator.
       This runs on each instance in the SciDB cluster.
     */
    shared_ptr< Array> execute(vector< shared_ptr< Array> >& inputArrays, shared_ptr<Query> query)
    {
        shared_ptr<Array> sa1 = inputArrays[0];
        shared_ptr<Array> sa2 = inputArrays[1];
        shared_ptr<ConstArrayIterator> saiter1(sa1->getConstIterator(0));
        shared_ptr<ConstArrayIterator> saiter2(sa2->getConstIterator(0));
        shared_ptr<ConstChunkIterator> sciter1;
        shared_ptr<ConstChunkIterator> sciter2;
        shared_ptr<Array> da(new MemArray(sa1->getArrayDesc(), query));
        shared_ptr<ArrayIterator> daiter(da->getIterator(0));
        shared_ptr<ChunkIterator> dciter;
        int n = ((boost::shared_ptr<OperatorParamPhysicalExpression>&)_parameters[0])->getExpression()->evaluate().getInt32();
        while (!saiter1->end() && !saiter2->end())
        {
            Coordinates const& chunkPos = saiters1->getPosition();
            sciter1 = saiter1->getChunk().getConstIterator(0);
            sciter2 = saiter2->getChunk().getConstIterator(0);
            dciter = daiter->newChunk(chunkPos).getIterator(query, ChunkIterator::SEQUENTIAL_WRITE);

            while(!sciter1->end() && !sciter2->end()) 
            {
                Coordinates const inputCellPos = sciter1->getPosition();
                Value const& val1 = sciter1->getItem();
                Value const& val2 = sciter2->getItem();
                Value val;
                double dval = val1.getDouble() + val2.getDouble();
                val.setData(dval, sizeof(double));

                dciter->setPosition(inputCellPos);
                dciter->writeItem(val);

                 ++(*sciter1);
                 ++(*sciter2);
                 ++(*dciter);
            }

            dciter->flush();
            if(dciter) dciter.reset();

            ++(*saiter1);
            ++(*saiter2);
            ++(*daiter);
        }
        return da;
    }
};
REGISTER_PHYSICAL_OPERATOR_FACTORY(Physicalknn, "polyfit", "PhysicalPolyfit");
} //namespace scidb
