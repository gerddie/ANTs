/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    $RCSfile: itkGaussianInterpolateImageFunction.h,v $
  Language:  C++
  Date:      $Date: 2009/07/01 12:59:34 $
  Version:   $Revision: 1.5 $

  Copyright (c) Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef __itkGaussianInterpolateImageFunction_h
#define __itkGaussianInterpolateImageFunction_h

#include "itkInterpolateImageFunction.h"
#include "vnl/vnl_erf.h"

namespace itk
{
/** \class GaussianInterpolateImageFunction
 * \brief Gaussianly interpolate an image at specified positions.
 *
 * GaussianInterpolateImageFunction linearly interpolates image intensity at
 * a non-integer pixel position. This class is templated
 * over the input image type and the coordinate representation type
 * (e.g. float or double).
 *
 * This function works for N-dimensional images.
 *
 * \ingroup ImageFunctions ImageInterpolators
 */
template <class TInputImage, class TCoordRep = double>
class ITK_EXPORT GaussianInterpolateImageFunction :
  public         InterpolateImageFunction<TInputImage, TCoordRep>
{
public:
  /** Standard class typedefs. */
  typedef GaussianInterpolateImageFunction                 Self;
  typedef InterpolateImageFunction<TInputImage, TCoordRep> Superclass;
  typedef SmartPointer<Self>                               Pointer;
  typedef SmartPointer<const Self>                         ConstPointer;

  /** Run-time type information (and related methods). */
  itkTypeMacro(GaussianInterpolateImageFunction, InterpolateImageFunction);

  /** Method for creation through the object factory. */
  itkNewMacro(Self);

  /** OutputType typedef support. */
  typedef typename Superclass::OutputType OutputType;

  /** InputImageType typedef support. */
  typedef typename Superclass::InputImageType InputImageType;

  /** RealType typedef support. */
  typedef typename Superclass::RealType RealType;

  /** Dimension underlying input image. */
  itkStaticConstMacro(VDim, unsigned int, Superclass::ImageDimension);

  /** Index typedef support. */
  typedef typename Superclass::IndexType IndexType;

  /** ContinuousIndex typedef support. */
  typedef typename Superclass::ContinuousIndexType ContinuousIndexType;

  /** Compute internals */
  virtual void ComputeBoundingBox()
  {
    const TInputImage *img = this->GetInputImage();

    if( img == NULL )
      {
      return;
      }
    // Set the bounding box
    for( size_t d = 0; d < VDim; d++ )
      {
      bb_start[d] = -0.5;
      bb_end[d] = img->GetBufferedRegion().GetSize()[d] - 0.5;
      nt[d] = (int)(bb_end[d] - bb_start[d] + 0.5);
      dx[d].set_size(nt[d]);
      gx[d].set_size(nt[d]);
      sf[d] = 1.0 / (sqrt(2.0) * sigma[d] / img->GetSpacing()[d]);
      cut[d] = sigma[d] * alpha / img->GetSpacing()[d];
      }
  }

  /** Set input */
  virtual void SetInputImage(const TInputImage *img)
  {
    // Call parent method
    Superclass::SetInputImage(img);

    this->ComputeBoundingBox();
  }

  void SetParameters(double *sigma, double alpha)
  {
    // Set the parameters
    for( size_t d = 0; d < VDim; d++ )
      {
      this->sigma[d] = sigma[d];
      }
    this->alpha = alpha;

    // If the image already set, recompute
    this->ComputeBoundingBox();
  }

  /** Evaluate the function at a ContinuousIndex position
   *
   * Returns the linearly interpolated image intensity at a
   * specified point position. No bounds checking is done.
   * The point is assume to lie within the image buffer.
   *
   * ImageFunction::IsInsideBuffer() can be used to check bounds before
   * calling the method. */
  virtual OutputType EvaluateAtContinuousIndex(
    const ContinuousIndexType & index ) const
  {
    return EvaluateAtContinuousIndex(index, NULL);
  }

  virtual OutputType EvaluateAtContinuousIndex(
    const ContinuousIndexType & index,
    OutputType *grad) const
  {
    // The bound variables for x, y, z
    int i0[VDim], i1[VDim];

    // Compute the ERF difference arrays
    for( size_t d = 0; d < VDim; d++ )
      {
      double *pdx = const_cast<double *>(dx[d].data_block() );
      double *pgx = grad ?  const_cast<double *>(gx[d].data_block() ) : NULL;
      compute_erf_array(pdx, i0[d], i1[d], bb_start[d], nt[d], cut[d], index[d], sf[d], pgx);
      }

    // Get a pointer to the output value
    double                         sum_me = 0.0, sum_m = 0.0;
    vnl_vector_fixed<double, VDim> dsum_me(0.0), dsum_m(0.0), dw;

    // Loop over the voxels in the region identified
    ImageRegion<VDim> region;
    for( size_t d = 0; d < VDim; d++ )
      {
      region.SetIndex(d, i0[d]);
      region.SetSize(d, i1[d] - i0[d]);
      }
    for(
      ImageRegionConstIteratorWithIndex<InputImageType> it(this->GetInputImage(), region);
      !it.IsAtEnd(); ++it )
      {
      size_t j = it.GetIndex()[0];
      double w = dx[0][j];
      if( grad )
        {
        dw[0] = gx[0][j];
        for( size_t d = 1; d < VDim; d++ )
          {
          dw[d] = dx[0][j];
          }
        }
      for( size_t d = 1; d < VDim; d++ )
        {
        j = it.GetIndex()[d];
        w *= dx[d][j];
        if( grad )
          {
          for( size_t q = 0; q < VDim; q++ )
            {
            dw[q] *= (d == q) ? gx[d][j] : dx[d][j];
            }
          }
        }

      double V = it.Get();
      sum_me += V * w;
      sum_m += w;
      if( grad )
        {
        for( size_t q = 0; q < VDim; q++ )
          {
          dsum_me[q] += V * dw[q];
          dsum_m[q] += dw[q];
          }
        }
      }

    double rc = sum_me / sum_m;
    if( grad )
      {
      for( size_t q = 0; q < VDim; q++ )
        {
        grad[q] = (dsum_me[q] - rc * dsum_m[q]) / sum_m;
        grad[q] /= -1.4142135623730951 * sigma[q];
        }
      }

    // return sum_me / sum_m;
    return rc;
  }

protected:
  GaussianInterpolateImageFunction()
  {
  }

  ~GaussianInterpolateImageFunction()
  {
  };
  void PrintSelf(std::ostream& os, Indent indent) const
  {
    this->Superclass::PrintSelf(os, indent);
  }

private:
  GaussianInterpolateImageFunction( const Self & ); // purposely not implemented
  void operator=( const Self & );                   // purposely not implemented

  /** Number of neighbors used in the interpolation */
  static const unsigned long m_Neighbors;

  vnl_vector<double> dx[VDim], gx[VDim];
  double             bb_start[VDim], bb_end[VDim], sf[VDim], cut[VDim];
  int                nt[VDim], stride[VDim];
  double             sigma[VDim], alpha;

  void compute_erf_array(
    double *dx_erf,               // The output array of erf(p+i+1) - erf(p+i)
    int & k0, int & k1,           // The range of integration 0 <= k0 < k1 <= n
    double b,                     // Lower bound of the bounding box
    int n,                        // Size of the bounding box in steps
    double cut,                   // The distance at which to cut off
    double p,                     // the value p
    double sfac,                  // scaling factor 1 / (Sqrt[2] sigma)
    double *gx_erf = NULL         // Output derivative/erf array (optional)
    ) const
  {
    // Determine the range of voxels along the line where to evaluate erf
    k0 = (int) floor(p - b - cut);
    k1 = (int) ceil(p - b + cut);
    if( k0 < 0 )
      {
      k0 = 0;
      }
    if( k1 > n )
      {
      k1 = n;
      }

    // Start at the first voxel
    double t = (b - p + k0) * sfac;
    double e_last = vnl_erf(t);
    double g_last = gx_erf ? 1.128379167095513 * exp(-t * t) : 0.0;
    for( int i = k0; i < k1; i++ )
      {
      t += sfac;
      double e_now = vnl_erf(t);
      dx_erf[i] = e_now - e_last;
      if( gx_erf )
        {
        double g_now = 1.128379167095513 * exp(-t * t);
        gx_erf[i] = g_now - g_last;
        g_last = g_now;
        }
      e_last = e_now;
      }
  }
};
} // end namespace itk

// Define instantiation macro for this template.
#define ITK_TEMPLATE_GaussianInterpolateImageFunction(_, EXPORT, x, y) namespace itk { \
  _(2 (class EXPORT GaussianInterpolateImageFunction<ITK_TEMPLATE_2 x> ) ) \
  namespace Templates { typedef GaussianInterpolateImageFunction<ITK_TEMPLATE_2 x> \
                          GaussianInterpolateImageFunction##y; } \
  }

#endif
