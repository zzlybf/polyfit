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
#include <limits>
#include <string>
#include <vector>

#include <system/Exceptions.h>
#include <query/TypeSystem.h>
#include <query/Operator.h>
#include <util/Platform.h>
#include <util/Network.h>
#include <boost/scope_exit.hpp>

#include <log4cxx/logger.h>

using std::shared_ptr;
using std::make_shared;

using namespace std;

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

    /**
      * [Optimizer API] Determine if operator changes result chunk distribution.
      * @param sourceSchemas shapes of all arrays that will given as inputs.
      * @return true if will changes output chunk distribution, false if otherwise
      */
    virtual bool changesDistribution(std::vector<ArrayDesc> const& sourceSchemas) const
    {
        return true;
    }
    
#ifdef CPP11
virtual RedistributeContext getOutputDistribution(std::vector<RedistributeContext> const&, std::vector<ArrayDesc> const&) const
{
	return RedistributeContext(createDistribution(psUndefined),_schema.getResidency());
}
#else
virtual ArrayDistribution getOutputDistribution(std::vector<ArrayDistribution> const&, std::vector<ArrayDesc> const&) const
{
	return ArrayDistribution(psUndefined);
}
#endif
    
void polyfit(int n,double x[],double y[],int poly_n,double a[])
{
    //void gauss_solve(int n,double A[],double x[],double b[]);
    int i,j;                                            
    double *tempx,*tempy,*sumxx,*sumxy,*ata;
    tempx=(double *)calloc(double(n),sizeof(double));   
    sumxx=(double *)calloc(poly_n*2+1,sizeof(double));
    tempy=(double *)calloc(n,sizeof(double));           
    sumxy=(double *)calloc(poly_n+1,sizeof(double));
    ata=(double *)calloc((poly_n+1)*(poly_n+1),sizeof(double));
    for (i=0;i<n;i++)
    {
        tempx[i]=1;
        tempy[i]=y[i];
    }
    for (i=0;i<2*poly_n+1;i++)
        for (sumxx[i]=0,j=0;j<n;j++)
        {
            sumxx[i]+=tempx[j];
            tempx[j]*=x[j];
        }
    for (i=0;i<poly_n+1;i++)
        for (sumxy[i]=0,j=0;j<n;j++)
        {
            sumxy[i]+=tempy[j];
            tempy[j]*=x[j];
        }
    for (i=0;i<poly_n+1;i++)
        for (j=0;j<poly_n+1;j++)
            ata[i*(poly_n+1)+j]=sumxx[i+j];
    gauss_solve(poly_n+1,ata,a,sumxy);
    free(tempx);    
    free(sumxx);
    free(tempy);    
    free(sumxy);
    free(ata);
}
void gauss_solve(int n,double A[],double x[],double b[])
{
    int i,j,k,r;
    double max;
    for (k=0;k<n-1;k++)
    {
        max=fabs(A[k*n+k]); /*find maxmum*/
        r=k;
        for (i=k+1;i<n-1;i++)
        if (max<fabs(A[i*n+i]))
        {
            max=fabs(A[i*n+i]);
            r=i;
        }
        if (r!=k)
            for (i=0;i<n;i++)         /*change array:A[k]&A[r]  */
            {
                max=A[k*n+i];
                A[k*n+i]=A[r*n+i];
                A[r*n+i]=max;
            }
            max=b[k];                    /*change array:b[k]&b[r]     */
            b[k]=b[r];
            b[r]=max;
            for (i=k+1;i<n;i++)
            {
                for (j=k+1;j<n;j++)
                    A[i*n+j]-=A[i*n+k]*A[k*n+j]/A[k*n+k];
                b[i]-=A[i*n+k]*b[k]/A[k*n+k];
            }
    }
    for (i=n-1;i>=0;x[i]/=A[i*n+i],i--)
        for (j=i+1,x[i]=b[i];j<n;j++)
            x[i]-=A[i*n+j]*x[j];
}

    /* The instance-parallel 'main' routine of this operator.
       This runs on each instance in the SciDB cluster.
     */ 
    shared_ptr< Array> execute(vector< shared_ptr< Array> >& inputArrays, shared_ptr<Query> query)
    { 
    	InstanceID const myId    = query->getInstanceID();
   	InstanceID const coordId = 0;

    	 if(myId != coordId)
    		return shared_ptr<Array>(new MemArray(_schema, query));
    		
        vector<double> x;
        vector<double> y;
        int n = ((boost::shared_ptr<OperatorParamPhysicalExpression>&)_parameters[0])->getExpression()->evaluate().getInt32();

        // set x array
        shared_ptr<Array> sa1 = inputArrays[0];
        shared_ptr<ConstArrayIterator> saiter1(sa1->getConstIterator(0));
        shared_ptr<ConstChunkIterator> sciter1;
        while (!saiter1->end())
        {
            sciter1 = saiter1->getChunk().getConstIterator(0);
            while(!sciter1->end()) 
            {
                Value const& val = sciter1->getItem();
                x.push_back(val.getDouble()); 
                 ++(*sciter1);
            }

            ++(*saiter1);
        }

        // set y array
        shared_ptr<Array> sa2 = inputArrays[1];
        shared_ptr<ConstArrayIterator> saiter2(sa2->getConstIterator(0));
        shared_ptr<ConstChunkIterator> sciter2;
        while (!saiter2->end())
        {
            sciter2 = saiter2->getChunk().getConstIterator(0);
            while(!sciter2->end()) 
            {
                Value const& val = sciter2->getItem();
                y.push_back(val.getDouble());   
                 ++(*sciter2);
            }

            ++(*saiter2);
        }
	
	
        // calculate result
        shared_ptr<Array> da(new MemArray(_schema, query));
        shared_ptr<ArrayIterator> daiter(da->getIterator(0));
        shared_ptr<ChunkIterator> dciter;

        Coordinates position(1,0);
        dciter = daiter->newChunk(position).getIterator(query, ChunkIterator::SEQUENTIAL_WRITE);
        dciter->setPosition(position);

       double a[n+1];
	 polyfit(x.size(), x.data(), y.data(), n, a);
	 
	 for (int i=n;i>=0;i--)
	 {
	     Value val;
            val.setData(&a[i],sizeof(double));
            dciter->writeItem(val);
            if(!dciter->end())
             	++(*dciter);
	 }
       dciter->flush();
       
        return da;
    }
   
    
};
REGISTER_PHYSICAL_OPERATOR_FACTORY(PhysicalPolyfit, "polyfit", "PhysicalPolyfit");
} //namespace scidb
