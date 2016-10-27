#include "helper.h"
#include "scalar_impl.h"


// compile time constants
constexpr int ndim          = 3;
constexpr int nnode         = 357602; 
constexpr int nelem         = 29998;
constexpr int nodeperelem   = 35;
constexpr int ngauss        = 31;

///
template<typename Ti, typename Tf, size_t ndim, size_t nelem, size_t nnode, size_t nodeperelem, size_t ngauss>
void run(const Tensor<Ti,nelem,nodeperelem> &elements, const Tensor<Tf,nnode,ndim> &points,
        const Tensor<Tf,ndim,nodeperelem,ngauss> &Jm, const Tensor<Tf,ngauss,1> &AllGauss) {

    volatile Tf u1 = random()+2;
    volatile Tf u2 = random()+1;
    volatile Tf kappa = random()+3;
    auto I = eye<Tf,ndim,ndim,ndim,ndim>();

    for (int elem=0; elem<nelem; ++elem) {
        Tensor<Tf,nodeperelem,ndim> LagrangeElemCoords;
        Tensor<Tf,nodeperelem,ndim> EulerElemCoords;

        for (int i=0; i<nodeperelem; ++i) {
            for (int j=0; j<ndim; ++j) {
                LagrangeElemCoords(i,j) = points(elements(elem,i),j);
                EulerElemCoords(i,j) = points(elements(elem,i),j);
            }
        }

        for (int g=0; g<ngauss; ++g) {
            // Get Gauss point Jm
            Tensor<Tf,ndim,nodeperelem> Jm_g;
            for (int i=0; i<ndim; ++i)
                for (int j=0; j<nodeperelem; ++j)
                    Jm_g(i,j) = Jm(i,j,g);

            // Compute gradient of shape functions
            auto ParentGradientX = sv::matmul(Jm_g,LagrangeElemCoords);
            // Compute material gradient
            auto MaterialGradient = sv::matmul(inverse(ParentGradientX),Jm_g);

            // Compute the deformation gradient tensor
            auto F = sv::matmul(MaterialGradient,EulerElemCoords);
            // Compute H
            auto H = cofactor(F);
            // auto H = sv::cofactor(F);
            // Comput J
            auto J = determinant(F);

            // Compute work-conjugates
            auto WF = sv::mul(2.*u1,F);
            auto WH = sv::mul(2.*u2,H);
            auto WJ = -2.*(u1+2*u2)/J+kappa*(J-1);
            // Compute first Piola-Kirchhoff stress tensor
            // auto P = WF + cross(static_cast<Tensor<Tf,ndim,ndim>>(WH),F) + WJ*H;
            auto P = sv::add(WF,sv::add(cross(static_cast<Tensor<Tf,ndim,ndim>>(WH),F), sv::mul(WJ,H)));

            // Compute Hessian components
            auto WFF = sv::mul(2.*u1,I);
            auto WHH = sv::mul(2.*u2,I);
            auto WJJ = 2./J/J*(u1+2*u2)+kappa;

            // dump
            unused(P);
            unused(WFF,WHH,WJJ);
        }
    }
}

FASTOR_NOINLINE void make_run() {

    std::string efile = "/home/roman/Dropbox/Fastor/benchmark/benchmark_academic/kernel_quadrature/meshes/mesh_hand_3d_elements_p4.dat";
    std::string pfile = "/home/roman/Dropbox/Fastor/benchmark/benchmark_academic/kernel_quadrature/meshes/mesh_hand_3d_points_p4.dat";
    std::string gfile0 = "/home/roman/Dropbox/Fastor/benchmark/benchmark_academic/kernel_quadrature/meshes/p4_3d_Jm.dat";
    std::string gfile1 = "/home/roman/Dropbox/Fastor/benchmark/benchmark_academic/kernel_quadrature/meshes/p4_3d_AllGauss.dat";


    Tensor<size_t,nelem,nodeperelem> elements = loadtxt<size_t,nelem,nodeperelem>(efile);
    Tensor<real,nnode,ndim> points = loadtxt<real,nnode,ndim>(pfile);
    // Fill with uniformly distributed random values
    decltype(points) Eulerx; Eulerx.random();

    Tensor<real,ndim*nodeperelem*ngauss,1> Jm_temp = loadtxt<real,ndim*nodeperelem*ngauss,1>(gfile0);
    Tensor<real,ndim,nodeperelem,ngauss> Jm;
    std::copy(Jm_temp.data(),Jm_temp.data()+Jm_temp.Size,Jm.data()); 
    Tensor<real,ngauss,1> AllGauss = loadtxt<real,ngauss,1>(gfile1);

    // check call
    run(elements,points,Jm,AllGauss);

    // run benchmark
    timeit(static_cast<void (*)(const Tensor<size_t,nelem,nodeperelem> &, const Tensor<real,nnode,ndim> &,
        const Tensor<real,ndim,nodeperelem,ngauss> &, const Tensor<real,ngauss,1> &)>(&run),elements,points,Jm,AllGauss);

}

int main() {

    //--------------------------------------------------------------------------------//
    const rlim_t stacksize = 512*1024*1024;
    struct rlimit rl;
    int result;
    result = getrlimit(RLIMIT_STACK, &rl);
    if (result==0) {
        if (rl.rlim_cur < stacksize) {
            rl.rlim_cur = stacksize;
            result = setrlimit(RLIMIT_STACK,&rl);
            if (result !=0) {
                FASTOR_ASSERT(result !=0, "CHANGING STACK SIZE FAILED");
            }
        }
    }
   //--------------------------------------------------------------------------------//

    make_run();

    return 0;
}