/**************************************************************************
**
**   SNOW - CS224 BROWN UNIVERSITY
**
**   implicit.cu
**   Authors: evjang, mliberma, taparson, wyegelwe
**   Created: 26 Apr 2014
**
**************************************************************************/

#ifndef IMPLICIT_H
#define IMPLICIT_H

#include <cuda.h>
#include <cuda_runtime.h>
#include <helper_functions.h>
#include <helper_cuda.h>

#define CUDA_INCLUDE
#include "geometry/grid.h"
#include "sim/material.h"
#include "sim/particle.h"
#include "sim/particlegridnode.h"
#include "cuda/vector.cu"

#include "cuda/atomic.cu"
#include "cuda/caches.h"
#include "cuda/decomposition.cu"
#include "cuda/weighting.cu"

#define BETA 0.5


/**
 * Called over particles
 **/
#define VEC2IVEC( V ) ( glm::ivec3((int)V.x, (int)V.y, (int)V.z) )
__global__ void computedF(Particle *particles, Grid *grid, float dt, ParticleGridNode *nodes, vec3 *dus, Implicit::ACache *ACaches){
    int particleIdx = blockIdx.y*gridDim.x*blockDim.x + blockIdx.x*blockDim.x + threadIdx.x;

    Particle &particle = particles[particleIdx];
    Implicit::ACache &ACache = ACaches[particleIdx];
    mat3 dF(0.0f);

    const vec3 &pos = particle.position;
    const glm::ivec3 &dim = grid->dim;
    const float h = grid->h;

    // Compute neighborhood of particle in grid
    vec3 gridIndex = (pos - grid->pos) / h,
         gridMax = vec3::floor( gridIndex + vec3(2,2,2) ),
         gridMin = vec3::ceil( gridIndex - vec3(2,2,2) );
    glm::ivec3 maxIndex = glm::clamp( VEC2IVEC(gridMax), glm::ivec3(0,0,0), dim ),
               minIndex = glm::clamp( VEC2IVEC(gridMin), glm::ivec3(0,0,0), dim );

    mat3 vGradient(0.0f);

    // Fill dF
    int rowSize = dim.z+1;
    int pageSize = (dim.y+1)*rowSize;
    for ( int i = minIndex.x; i <= maxIndex.x; ++i ) {
        vec3 d, s;
        d.x = gridIndex.x - i;
        d.x *= ( s.x = ( d.x < 0 ) ? -1.f : 1.f );
        int pageOffset = i*pageSize;
        for ( int j = minIndex.y; j <= maxIndex.y; ++j ) {
            d.y = gridIndex.y - j;
            d.y *= ( s.y = ( d.y < 0 ) ? -1.f : 1.f );
            int rowOffset = pageOffset + j*rowSize;
            for ( int k = minIndex.z; k <= maxIndex.z; ++k ) {
                d.z = gridIndex.z - k;
                d.z *= ( s.z = ( d.z < 0 ) ? -1.f : 1.f );
                vec3 wg;
                weightGradient( -s, d, wg );

                vec3 du_j = dt * dus[rowOffset+k];
                dF += mat3::outerProduct(du_j, wg);

                vGradient += mat3::outerProduct(dt*nodes[rowOffset+k].velocity, wg);

            }
        }
    }

    ACache.dF = dF * particle.elasticF;

    ACache.FeHat = mat3::addIdentity(vGradient) * particle.elasticF;
    computePD(ACache.FeHat, ACache.ReHat, ACache.SeHat);
}

/** Currently computed in computedF, we could parallelize this and computedF but not sure what the time benefit would be*/
//__global__ void computeFeHat(Particle *particles, Grid *grid, float dt, ParticleGridNode *nodes, ACache *ACaches){
//    int particleIdx = blockIdx.x*blockDim.x + threadIdx.x;

//       Particle &particle = particles[particleIdx];
//       ACache &ACache = ACaches[particleIdx];

//       vec3 particleGridPos = (particle.position - grid->pos) / grid->h;
//       glm::ivec3 min = glm::ivec3(std::ceil(particleGridPos.x - 2), std::ceil(particleGridPos.y - 2), std::ceil(particleGridPos.z - 2));
//       glm::ivec3 max = glm::ivec3(std::floor(particleGridPos.x + 2), std::floor(particleGridPos.y + 2), std::floor(particleGridPos.z + 2));

//       mat3 vGradient(0.0f);

//       // Apply particles contribution of mass, velocity and force to surrounding nodes
//       min = glm::max(glm::ivec3(0.0f), min);
//       max = glm::min(grid->dim, max);
//       for (int i = min.x; i <= max.x; i++){
//           for (int j = min.y; j <= max.y; j++){
//               for (int k = min.z; k <= max.z; k++){
//                   int currIdx = grid->getGridIndex(i, j, k, grid->dim+1);
//                   ParticleGridNode &node = nodes[currIdx];

//                   vec3 wg;
//                   weightGradient(particleGridPos - vec3(i, j, k), wg);

//                   vGradient += mat3::outerProduct(dt*node.velocity, wg);
//               }
//           }
//       }

//       ACache.FeHat = mat3::addIdentity(vGradient) * particle.elasticF;
//       computePD(ACache.FeHat, ACache.ReHat, ACache.SeHat);
//}

__device__ void computedR(mat3 &dF, mat3 &Se, mat3 &Re, mat3 &dR){
    mat3 V = mat3::multiplyAtB(Re, dF) - mat3::multiplyAtB(dF, Re);

    // Solve for compontents of R^T * dR
    mat3 A = mat3(S[0]+S[4], S[5], -S[2], //remember, column major
                  S[5], S[0]+S[8], S[1],
                  -S[2], S[1], S[4]+S[8]);

    vec3 b(V[3], V[6], V[7]);
    vec3 x = mat3::solve(A, b);// Should replace this with a linear system solver function

    // Fill R^T * dR
    mat3 RTdR = mat3( 0, -x.x, -x.y, //remember, column major
                      x.x, 0, -x.z,
                      x.y, x.z, 0);

    dR = Re*RTdR;
}

/**
 * This function involves taking the partial derivative of the adjugate of F
 * with respect to each element of F. This process results in a 3x3 block matrix
 * where each block is the 3x3 partial derivative for an element of F
 *
 * Let F = [ a b c
 *           d e f
 *           g h i ]
 *
 * Let adjugate(F) = [ ei-hf  hc-bi  bf-ec
 *                     gf-di  ai-gc  dc-af
 *                     dh-ge  gb-ah  ae-db ]
 *
 * Then d/da (adjugate(F) = [ 0   0   0
 *                            0   i  -f
 *                            0  -h   e ]
 *
 * The other 8 partials will have similar form. See (and run) the code in
 * matlab/derivateAdjugateF.m for the full computation as well as to see where
 * these seemingly magic values came from.
 *
 *
 */
__device__ void compute_dJF_invTrans(mat3 &F, mat3 &dF, mat3 &dJF_invTrans){
    dJF_invTrans[0] = F[4]*dF[8] - F[5]*dF[5] + F[8]*dF[4] - F[7]*dF[7];
    dJF_invTrans[1] = F[5]*dF[2] - F[8]*dF[1] - F[3]*dF[8] + F[6]*dF[7];
    dJF_invTrans[2] = F[3]*dF[5] - F[4]*dF[2] + F[7]*dF[1] - F[6]*dF[4];
    dJF_invTrans[3] = F[2]*dF[5] - F[1]*dF[8] - F[8]*dF[3] + F[7]*dF[6];
    dJF_invTrans[4] = F[0]*dF[8] - F[2]*dF[2] + F[8]*dF[0] - F[6]*dF[6];
    dJF_invTrans[5] = F[1]*dF[2] - F[0]*dF[5] - F[7]*dF[0] + F[6]*dF[3];
    dJF_invTrans[6] = F[1]*dF[7] - F[2]*dF[4] + F[5]*dF[3] - F[4]*dF[6];
    dJF_invTrans[7] = F[2]*dF[1] - F[5]*dF[0] - F[0]*dF[7] + F[3]*dF[6];
    dJF_invTrans[8] = F[0]*dF[4] - F[1]*dF[1] + F[4]*dF[0] - F[3]*dF[3];
}

/**
 * Called over particles
 **/
// TODO: Replace JFe_invTrans with the trans of adjugate
__global__ void computeAp(Particle *particles, MaterialConstants *material, Implicit::ACache *ACaches)
{
    int particleIdx = blockIdx.y*gridDim.x*blockDim.x + blockIdx.x*blockDim.x + threadIdx.x;
    Particle &particle = particles[particleIdx];
    Implicit::ACache &ACache = ACaches[particleIdx];
    mat3 dF = ACache.dF;

    mat3 &Fp = particle.plasticF; //for the sake of making the code look like the math
    mat3 &Fe = ACache.FeHat;

    float Jpp = mat3::determinant(Fp);
    float Jep = mat3::determinant(Fe);

    float muFp = material->mu*__expf(material->xi*(1-Jpp));
    float lambdaFp = material->lambda*__expf(material->xi*(1-Jpp));

    mat3 &Re = ACache.ReHat;
    mat3 &Se = ACache.SeHat;

    mat3 dR;
    computedR(dF, Se, Re, dR);

    mat3 dJFe_invTrans;
    compute_dJF_invTrans(Fe, dF, dJFe_invTrans);

    mat3 jFe_invTrans = Jep * mat3::transpose(mat3::inverse(Fe));

    ACache.Ap = (2*muFp*(dF - dR) + lambdaFp*jFe_invTrans*mat3::innerProduct(jFe_invTrans, dF) + lambdaFp*(Jep - 1)*dJFe_invTrans);
}


__global__ void computedf( Particle *particles, Grid *grid, mat3 *Aps, vec3 *dfs )
{
    int particleIdx = blockIdx.y*gridDim.x*blockDim.x + blockIdx.x*blockDim.x + threadIdx.x;

    Particle &particle = particles[particleIdx];
    vec3 gridPos = (particle.position-grid->pos)/grid->h;

    glm::ivec3 ijk;
    Grid::gridIndexToIJK( threadIdx.y, glm::ivec3(4,4,4), ijk );
    ijk += glm::ivec3( gridPos.x-1, gridPos.y-1, gridPos.z-1 );

    if ( Grid::withinBoundsInclusive(ijk, glm::ivec3(0,0,0), grid->dim) ) {

        vec3 wg;
        vec3 nodePos(ijk);
        weightGradient( gridPos-nodePos, wg );

        const mat3 &Ap = Aps[particleIdx];
        vec3 df = -particle.volume * mat3::multiplyABt( Ap, particle.elasticF ) * wg;

        atomicAdd( &dfs[Grid::getGridIndex(ijk,grid->nodeDim())], df );
    }
}

__global__ void computeResult( ParticleGridNode *nodes, int numNodes, float dt, const vec3 *u, const vec3 *dfs, vec3 *result )
{
    int tid = blockDim.x*blockIdx.x + threadIdx.x;
    if ( tid >= numNodes ) return;
    result[tid] = u[tid] - (BETA*dt/nodes[tid].mass)*dfs[tid];
}

/**
 * Computes the matrix-vector product Eu. All the pointer arguments are assumed to be
 * device pointers.
 *
 *      u:  device pointer to vector to multiply
 *    dFs:  device pointer to storage for per-particle dF matrices
 *    Aps:  device pointer to storage for per-particle Ap matrices
 * result:  device pointer to array store the values of Eu
 */
__host__ void computeEu( Particle *particles, int numParticles,
                         Grid *grid, ParticleGridNode *nodes, int numNodes,
                         float dt, vec3 *u, mat3 *dFs, mat3 *Aps, vec3 *dfs, vec3 *result )
{
    static const int threadCount = 128;

    dim3 blocks = dim3( numParticles/threadCount, 64 );
    dim3 threads = dim3( threadCount/64, 64 );
    computedf<<< blocks, threads >>>( particles, grid, Aps, dfs );
    checkCudaErrors( cudaDeviceSynchronize() );

    computeResult<<< numNodes/threadCount, threadCount >>>( nodes, numNodes, dt, u, dfs, result );
    checkCudaErrors( cudaDeviceSynchronize() );
}

__host__ void conjugateResidual( Particle *particles, int numParticles,
                                 Grid *grid, ParticleGridNode *nodes, int numNodes,
                                 float dt, vec3 *u, mat3 *dFs, mat3 *Aps, vec3 *dfs, vec3 *result )
{

}

#endif // IMPLICIT_H
