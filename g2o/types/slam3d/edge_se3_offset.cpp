#include "edge_se3_offset.h"
#include "isometry3d_gradients.h"
#include "parameter_se3_offset.h"

#include <iostream>

namespace g2o {
  using namespace std;

  EdgeSE3Offset::EdgeSE3Offset() : BaseBinaryEdge<6, SE3Quat, VertexSE3, VertexSE3>() {
    information().setIdentity();
    _offsetFrom = 0;
    _offsetTo = 0;
    _cacheFrom = 0;
    _cacheTo = 0;
    resizeParameters(2);
    installParameter(_offsetFrom, 0);
    installParameter(_offsetTo, 1);
  }

  bool EdgeSE3Offset::resolveCaches(){
    assert(_offsetFrom && _offsetTo);

    ParameterVector pv(2);
    pv[0]=_offsetFrom;
    resolveCache(_cacheFrom, (OptimizableGraph::Vertex*)_vertices[0],"CACHE_SE3_OFFSET",pv);
    pv[1]=_offsetTo;
    resolveCache(_cacheTo, (OptimizableGraph::Vertex*)_vertices[1],"CACHE_SE3_OFFSET",pv);
    return (_cacheFrom && _cacheTo);
  }

  bool EdgeSE3Offset::read(std::istream& is) {
    int pidFrom, pidTo;
    is >> pidFrom >> pidTo   ;
    if (! setParameterId(0,pidFrom))
      return false;
    if (! setParameterId(1,pidTo))
      return false;

    Vector7d meas;
    for (int i=0; i<7; i++) 
      is >> meas[i];
    setMeasurement(SE3Quat(meas));
    
    if (is.bad()) {
      return false;
    }
    for ( int i=0; i<information().rows() && is.good(); i++)
      for (int j=i; j<information().cols() && is.good(); j++){
  is >> information()(i,j);
  if (i!=j)
    information()(j,i)=information()(i,j);
      }
    if (is.bad()) {
      //  we overwrite the information matrix with the Identity
      information().setIdentity();
    } 
    return true;
  }

  bool EdgeSE3Offset::write(std::ostream& os) const {
    os << _offsetFrom->id() << " " << _offsetTo->id() << " ";
    for (int i=0; i<7; i++) os  << measurement()[i] << " ";
    for (int i=0; i<information().rows(); i++)
      for (int j=i; j<information().cols(); j++) {
        os <<  information()(i,j) << " ";
      }
    return os.good();
  }

  void EdgeSE3Offset::computeError() {
    SE3Quat delta=_inverseMeasurement * _cacheFrom->w2n() * _cacheTo->n2w();
    _error.head<3>() = delta.translation();
    // The analytic Jacobians assume the error in this special form (w beeing positive)
    if (delta.rotation().w() < 0.)
      _error.tail<3>() =  - delta.rotation().vec();
    else
      _error.tail<3>() =  delta.rotation().vec();
  }

  bool EdgeSE3Offset::setMeasurementFromState(){
    SE3Quat delta = _cacheFrom->w2n() * _cacheTo->n2w();
    setMeasurement(delta);
    return true;
  }

  void EdgeSE3Offset::linearizeOplus(){
    //BaseBinaryEdge<6, SE3Quat, VertexSE3, VertexSE3>::linearizeOplus();

    VertexSE3 *from = static_cast<VertexSE3*>(_vertices[0]);
    VertexSE3 *to   = static_cast<VertexSE3*>(_vertices[1]);
    Eigen::Isometry3d E;
    Eigen::Isometry3d Z, Xi, Xj, Pi, Pj;
    Xi=from->estimate().rotation().toRotationMatrix();
    Xi.translation()=from->estimate().translation();
    Xj=to->estimate().rotation().toRotationMatrix();
    Xj.translation()=to->estimate().translation();
    Pi=_cacheFrom->offsetParam()->offset().rotation().toRotationMatrix();
    Pi.translation()=_cacheFrom->offsetParam()->offset().translation();
    Pj=_cacheTo->offsetParam()->offset().rotation().toRotationMatrix();
    Pj.translation()=_cacheTo->offsetParam()->offset().translation();
    Z=_measurement.rotation().toRotationMatrix();
    Z.translation()=_measurement.translation();
    // Matrix6d Ji, Jj;
    // computeSE3Gradient(E, Ji , Jj,
    //                    Z, Pi, Xi, Pj, Xj);
    // cerr  << "Ji:" << endl;
    // cerr << Ji-_jacobianOplusXi << endl;
    // cerr  << "Jj:" << endl;
    // cerr << Jj-_jacobianOplusXj << endl;
    computeEdgeSE3Gradient(E, _jacobianOplusXi , _jacobianOplusXj,
                       Z, Xi, Xj, Pi, Pj);
  }

  void EdgeSE3Offset::initialEstimate(const OptimizableGraph::VertexSet& from_, OptimizableGraph::Vertex* /*to_*/) {
    VertexSE3 *from = static_cast<VertexSE3*>(_vertices[0]);
    VertexSE3 *to   = static_cast<VertexSE3*>(_vertices[1]);

    SE3Quat virtualMeasurement = _cacheFrom->offsetParam()->offset() * measurement() * _cacheTo->offsetParam()->offset().inverse();
    
    if (from_.count(from) > 0) {
      to->setEstimate(from->estimate() * virtualMeasurement);
    } else
      from->setEstimate(to->estimate() * virtualMeasurement.inverse());
  }

}