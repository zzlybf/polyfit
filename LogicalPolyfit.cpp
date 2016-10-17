/**
 * @file Logicalknn.cpp
 *
 * @brief Brute force k nearest neighbors.
 *
 * @par Synopsis: knn(A, k).
 *  
 * @par Summary:
 * Simple brute force k nearest neighbor enumeration for a full distance
 * matrix. When used without a generic matrix input, it simply identifies
 * the k smallest values per row.
 * <br>
 *
 * @par Input:
 * A is a 2-d full distance matrix with one double precision-valued attribute
 * chunked only along rows k number of nearest neighbors to identify
 * <br>
 *
 * @par Output array:
 * Has same schema as A, but is sparse with the k nearest neighbors
 * enumerated.
 * <br>
 *
 * @par Examples:
 * See help('knn')
 * <br>
 *
 * @author B. W. Lewis <blewis@paradigm4.com>
 */

#include "query/Operator.h"
namespace scidb
{

class LogicalPolyfit : public LogicalOperator
{
public:
    LogicalPolyfit(const string& logicalName, const string& alias):
        LogicalOperator(logicalName, alias)
    {
        ADD_PARAM_INPUT()
        ADD_PARAM_INPUT()
        ADD_PARAM_CONSTANT("int32")
        _usage = "polyfit(x, y, n)\n";}

/* inferSchema helps the query planner decide on the shape of
 * the output array. All operators must define this function.
 */
    ArrayDesc inferSchema(vector< ArrayDesc> schemas, shared_ptr< Query> query)
    {
        ArrayDesc const& matrix = schemas[0];
        if(matrix.getAttributes(true)[0].getType() != TID_DOUBLE)
           throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) <<  "polyfit first argument requires a single double precision-valued attribute";
        if(matrix.getDimensions().size() != 1 )
           throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) <<  "polyfit first argument requires an array input";
        if (matrix.getDimensions()[0].getChunkInterval() != static_cast<int64_t>(matrix.getDimensions()[0].getLength()))
            throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "polyfit first argument does not accept column partitioning of the input array, use repart first";        
        matrix = schemas[1];
        if(matrix.getAttributes(true)[0].getType() != TID_DOUBLE)
           throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) <<  "polyfit second argument requires a single double precision-valued attribute";
        if(matrix.getDimensions().size() != 1 )
           throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) <<  "polyfit second argument requires an array input";
        if (matrix.getDimensions()[0].getChunkInterval() != static_cast<int64_t>(matrix.getDimensions()[0].getLength()))
            throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "polyfit second argument does not accept column partitioning of the input array, use repart first";
                
        Attributes outputAttributes(matrix.getAttributes());
        Dimensions outputDimensions(matrix.getDimensions());
        return ArrayDesc("polyfit_array", outputAttributes, outputDimensions);
    }
};

REGISTER_LOGICAL_OPERATOR_FACTORY(LogicalPolyfit, "polyfit");

}
