/* $Header$ */
/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2007 Robert McNeel & Associates. All rights reserved.
// Rhinoceros is a registered trademark of Robert McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

#if !defined(ON_GEOMETRY_SURFACE_PLANE_INC_)
#define ON_GEOMETRY_SURFACE_PLANE_INC_

class ON_PlaneSurface;

typedef ON_Mesh* (*ON_MeshPlaneSurface)( const ON_PlaneSurface&, const ON_MeshParameters&, ON_Mesh* );

class ON_CLASS ON_PlaneSurface : public ON_Surface
{
  ON_OBJECT_DECLARE(ON_PlaneSurface);

public:
  ON_PlaneSurface();
  ON_PlaneSurface(const ON_PlaneSurface&);
  ON_PlaneSurface& operator=(const ON_PlaneSurface&);

  ON_PlaneSurface(const ON_Plane&);
  ON_PlaneSurface& operator=(const ON_Plane&);

  virtual ~ON_PlaneSurface();

  // An ON_PlaneSurface is really a finite rectangle.
  // m_plane defines the plane and m_extents[] stores
  // the x and y intervals of the plane that define the
  // rectangle.  The m_domain[] intervals specify the
  // evaluation domain.  Changing the extents are domain
  // are INDEPENDENT of each other.  Use Domain() and
  // SetDomain() to control the evluation domain.  Use
  // Extents() and SetExtents() to control the rectangle
  // extents.
  ON_Plane m_plane;

  /////////////////////////////////////////////////////////////////
  // ON_Object overrides

  // virtual ON_Object::SizeOf override
  unsigned int SizeOf() const;

  // virtual ON_Object::DataCRC override
  ON__UINT32 DataCRC(ON__UINT32 current_remainder) const;

  /*
  Description:
    Tests an object to see if its data members are correctly
    initialized.
  Parameters:
    text_log - [in] if the object is not valid and text_log
	is not NULL, then a brief englis description of the
	reason the object is not valid is appened to the log.
	The information appended to text_log is suitable for 
	low-level debugging purposes by programmers and is 
	not intended to be useful as a high level user 
	interface tool.
  Returns:
    @untitled table
    TRUE     object is valid
    FALSE    object is invalid, uninitialized, etc.
  Remarks:
    Overrides virtual ON_Object::IsValid
  */
  BOOL IsValid( ON_TextLog* text_log = NULL ) const;

  void Dump( ON_TextLog& ) const; // for debugging

  BOOL Write(
	 ON_BinaryArchive&  // open binary file
       ) const;

  BOOL Read(
	 ON_BinaryArchive&  // open binary file
       );

  /////////////////////////////////////////////////////////////////
  // ON_Geometry overrides

  int Dimension() const;

  BOOL GetBBox( // returns TRUE if successful
	 double*,    // minimum
	 double*,    // maximum
	 BOOL = FALSE  // TRUE means grow box
	 ) const;

  BOOL Transform( 
	 const ON_Xform&
	 );

  /////////////////////////////////////////////////////////////////
  // ON_Surface overrides

  ON_Mesh* CreateMesh( 
	     const ON_MeshParameters& mp,
	     ON_Mesh* mesh = NULL
	     ) const;

  /*
  Description:
    Sets the evaluation domains.  Does not change the geometry.
  Parameters:
    dir - [in] 0 sets first parameter's domain
	       1 sets second parameter's domain
    t0 - [in]
    t1 - [in] (t0 < t1) the interval (t0,t1) will be the new domain
  Returns:
    True if successful.
  See Also:
    ON_PlaneSurface::SetExtents
  */
  BOOL SetDomain( 
    int dir, 
    double t0, 
    double t1
    );

  ON_Interval Domain(
    int // 0 gets first parameter's domain, 1 gets second parameter's domain
    ) const;

  /*
  Description:
    Get an estimate of the size of the rectangle that would
    be created if the 3d surface where flattened into a rectangle.
  Parameters:
    width - [out]  (corresponds to the first surface parameter)
    height - [out] (corresponds to the first surface parameter)
  Remarks:
    overrides virtual ON_Surface::GetSurfaceSize
  Returns:
    TRUE if successful.
  */
  BOOL GetSurfaceSize( 
      double* width, 
      double* height 
      ) const;

  int SpanCount(
    int // 0 gets first parameter's domain, 1 gets second parameter's domain
    ) const; // number of smooth spans in curve

  BOOL GetSpanVector( // span "knots" 
    int, // 0 gets first parameter's domain, 1 gets second parameter's domain
    double* // array of length SpanCount() + 1 
    ) const; // 

  int Degree( // returns maximum algebraic degree of any span 
		  // ( or a good estimate if curve spans are not algebraic )
    int // 0 gets first parameter's domain, 1 gets second parameter's domain
    ) const; 

  BOOL GetParameterTolerance( // returns tminus < tplus: parameters tminus <= s <= tplus
	 int,     // 0 gets first parameter, 1 gets second parameter
	 double,  // t = parameter in domain
	 double*, // tminus
	 double*  // tplus
	 ) const;

  /*
  Description:
    Test a surface to see if it is planar.
  Parameters:
    plane - [out] if not NULL and TRUE is returned,
		  the plane parameters are filled in.
    tolerance - [in] tolerance to use when checking
  Returns:
    TRUE if there is a plane such that the maximum distance from
    the surface to the plane is <= tolerance.
  Remarks:
    Overrides virtual ON_Surface::IsPlanar.
  */
  BOOL IsPlanar(
	ON_Plane* plane = NULL,
	double tolerance = ON_ZERO_TOLERANCE
	) const;

  BOOL IsClosed(   // TRUE if surface is closed in direction
	int        // dir  0 = "s", 1 = "t"
	) const;

  BOOL IsPeriodic( // TRUE if surface is periodic in direction
	int        // dir  0 = "s", 1 = "t"
	) const;

  BOOL IsSingular( // TRUE if surface side is collapsed to a point
	int        // side of parameter space to test
		   // 0 = south, 1 = east, 2 = north, 3 = west
	) const;
  
  /*
  Description:
    Search for a derivatitive, tangent, or curvature 
    discontinuity.
  Parameters:
    dir - [in] If 0, then "u" parameter is checked.  If 1, then
	       the "v" parameter is checked.
    c - [in] type of continity to test for.
    t0 - [in] Search begins at t0. If there is a discontinuity
	      at t0, it will be ignored.  This makes it 
	      possible to repeatedly call GetNextDiscontinuity
	      and step through the discontinuities.
    t1 - [in] (t0 != t1)  If there is a discontinuity at t1 is 
	      will be ingored unless c is a locus discontinuity
	      type and t1 is at the start or end of the curve.
    t - [out] if a discontinuity is found, then *t reports the
	  parameter at the discontinuity.
    hint - [in/out] if GetNextDiscontinuity will be called 
       repeatedly, passing a "hint" with initial value *hint=0
       will increase the speed of the search.       
    dtype - [out] if not NULL, *dtype reports the kind of 
	discontinuity found at *t.  A value of 1 means the first 
	derivative or unit tangent was discontinuous.  A value 
	of 2 means the second derivative or curvature was 
	discontinuous.  A value of 0 means teh curve is not
	closed, a locus discontinuity test was applied, and
	t1 is at the start of end of the curve.
    cos_angle_tolerance - [in] default = cos(1 degree) Used only
	when c is ON::G1_continuous or ON::G2_continuous.  If the
	cosine of the angle between two tangent vectors is 
	<= cos_angle_tolerance, then a G1 discontinuity is reported.
    curvature_tolerance - [in] (default = ON_SQRT_EPSILON) Used 
	only when c is ON::G2_continuous.  If K0 and K1 are 
	curvatures evaluated from above and below and 
	|K0 - K1| > curvature_tolerance, then a curvature 
	discontinuity is reported.
  Returns:
    Parametric continuity tests c = (C0_continuous, ..., G2_continuous):

      TRUE if a parametric discontinuity was found strictly 
      between t0 and t1. Note well that all curves are 
      parametrically continuous at the ends of their domains.

    Locus continuity tests c = (C0_locus_continuous, ...,G2_locus_continuous):

      TRUE if a locus discontinuity was found strictly between
      t0 and t1 or at t1 is the at the end of a curve.
      Note well that all open curves (IsClosed()=false) are locus
      discontinuous at the ends of their domains.  All closed 
      curves (IsClosed()=true) are at least C0_locus_continuous at 
      the ends of their domains.
  */
  bool GetNextDiscontinuity( 
		  int dir,
		  ON::continuity c,
		  double t0,
		  double t1,
		  double* t,
		  int* hint=NULL,
		  int* dtype=NULL,
		  double cos_angle_tolerance=0.99984769515639123915701155881391,
		  double curvature_tolerance=ON_SQRT_EPSILON
		  ) const;

  /*
  Description:
    Test continuity at a surface parameter value.
  Parameters:
    c - [in] continuity to test for
    s - [in] surface parameter to test
    t - [in] surface parameter to test
    hint - [in] evaluation hint
    point_tolerance - [in] if the distance between two points is
	greater than point_tolerance, then the surface is not C0.
    d1_tolerance - [in] if the difference between two first derivatives is
	greater than d1_tolerance, then the surface is not C1.
    d2_tolerance - [in] if the difference between two second derivatives is
	greater than d2_tolerance, then the surface is not C2.
    cos_angle_tolerance - [in] default = cos(1 degree) Used only when
	c is ON::G1_continuous or ON::G2_continuous.  If the cosine
	of the angle between two normal vectors 
	is <= cos_angle_tolerance, then a G1 discontinuity is reported.
    curvature_tolerance - [in] (default = ON_SQRT_EPSILON) Used only when
	c is ON::G2_continuous.  If K0 and K1 are curvatures evaluated
	from above and below and |K0 - K1| > curvature_tolerance,
	then a curvature discontinuity is reported.
  Returns:
    TRUE if the surface has at least the c type continuity at the parameter t.
  Remarks:
    Overrides virtual ON_Surface::IsContinuous
  */
  bool IsContinuous(
    ON::continuity c,
    double s, 
    double t, 
    int* hint = NULL,
    double point_tolerance=ON_ZERO_TOLERANCE,
    double d1_tolerance=ON_ZERO_TOLERANCE,
    double d2_tolerance=ON_ZERO_TOLERANCE,
    double cos_angle_tolerance=0.99984769515639123915701155881391,
    double curvature_tolerance=ON_SQRT_EPSILON
    ) const;

  BOOL Reverse(  // reverse parameterizatrion, Domain changes from [a,b] to [-b,-a]
    int // dir  0 = "s", 1 = "t"
    );

  BOOL Transpose(); // transpose surface parameterization (swap "s" and "t")


  BOOL Evaluate( // returns FALSE if unable to evaluate
	 double, double, // evaluation parameters
	 int,            // number of derivatives (>=0)
	 int,            // array stride (>=Dimension())
	 double*,        // array of length stride*(ndir+1)*(ndir+2)/2
	 int = 0,        // optional - determines which quadrant to evaluate from
			 //         0 = default
			 //         1 from NE quadrant
			 //         2 from NW quadrant
			 //         3 from SW quadrant
			 //         4 from SE quadrant
	 int* = 0        // optional - evaluation hint (int[2]) used to speed
			 //            repeated evaluations
	 ) const;

  /*
  Description:
    Get isoparametric curve.
    Overrides virtual ON_Surface::IsoCurve.
  Parameters:
    dir - [in] 0 first parameter varies and second parameter is constant
		 e.g., point on IsoCurve(0,c) at t is srf(t,c)
	       1 first parameter is constant and second parameter varies
		 e.g., point on IsoCurve(1,c) at t is srf(c,t)

    c - [in] value of constant parameter 
  Returns:
    Isoparametric curve.
  */
  ON_Curve* IsoCurve(
	 int dir,         
	 double c
	 ) const;

  /*
  Description:
    Compute a 3d curve that is the composite of a 2d curve
    and the surface map.
  Parameters:
    curve_2d - [in] a 2d curve whose image is in the surface's domain.
    tolerance - [in] the maximum acceptable distance from the returned
       3d curve to the image of curve_2d on the surface.
    curve_2d_subdomain - [in] optional subdomain for curve_2d
  Returns:
    3d curve.
  See Also:
    ON_Surface::IsoCurve
    ON_Surface::Pullback
  Remarks:
    Overrides virtual ON_Surface::Pushup.
  */
  ON_Curve* Pushup( const ON_Curve& curve_2d,
		    double tolerance,
		    const ON_Interval* curve_2d_subdomain = NULL
		    ) const;

  /*
  Description:
    Pull a 3d curve back to the surface's parameter space.
  Parameters:
    curve_3d - [in] a 3d curve
    tolerance - [in] the maximum acceptable 3d distance between
       from surface(curve_2d(t)) to the locus of points on the
       surface that are closest to curve_3d.
    curve_3d_subdomain - [in] optional subdomain for curve_3d
    start_uv - [in] optional starting point (if known)
    end_uv - [in] optional ending point (if known)
  Returns:
    2d curve.
  See Also:
    ON_Surface::IsoCurve
    ON_Surface::Pushup
  Remarks:
    Overrides virtual ON_Surface::Pullback.
  */
  ON_Curve* Pullback( const ON_Curve& curve_3d,
		    double tolerance,
		    const ON_Interval* curve_3d_subdomain = NULL,
		    ON_3dPoint start_uv = ON_UNSET_POINT,
		    ON_3dPoint end_uv = ON_UNSET_POINT
		    ) const;

  /*
  Description:
    Removes the portions of the surface outside of the specified interval.
    Overrides virtual ON_Surface::Trim.

  Parameters:
    dir - [in] 0  The domain specifies an sub-interval of Domain(0)
		  (the first surface parameter).
	       1  The domain specifies an sub-interval of Domain(1)
		  (the second surface parameter).
    domain - [in] interval of the surface to keep. If dir is 0, then
	the portions of the surface with parameters (s,t) satisfying
	s < Domain(0).Min() or s > Domain(0).Max() are trimmed away.
	If dir is 1, then the portions of the surface with parameters
	(s,t) satisfying t < Domain(1).Min() or t > Domain(1).Max() 
	are trimmed away.
  */
  BOOL Trim(
	 int dir,
	 const ON_Interval& domain
	 );

  /*
   Description:
     Where possible, analytically extends surface to include domain.
   Parameters:
     dir - [in] 0  new Domain(0) will include domain.
		   (the first surface parameter).
		1  new Domain(1) will include domain.
		   (the second surface parameter).
     domain - [in] if domain is not included in surface domain, 
     surface will be extended so that its domain includes domain.  
     Will not work if surface is closed in direction dir. 
     Original surface is identical to the restriction of the
     resulting surface to the original surface domain, 
   Returns:
     true if successful.
     */
  bool Extend(
    int dir,
    const ON_Interval& domain
    );

  /*
  Description:
    Splits (divides) the surface into two parts at the 
    specified parameter.
    Overrides virtual ON_Surface::Split.

  Parameters:
    dir - [in] 0  The surface is split vertically.  The "west" side
		  is returned in "west_or_south_side" and the "east"
		  side is returned in "east_or_north_side".
	       1  The surface is split horizontally.  The "south" side
		  is returned in "west_or_south_side" and the "north"
		  side is returned in "east_or_north_side".
    c - [in] value of constant parameter in interval returned
	       by Domain(dir)
    west_or_south_side - [out] west/south portion of surface returned here
    east_or_north_side - [out] east/north portion of surface returned here

  Example:

	  ON_PlaneSurface srf = ...;
	  int dir = 1;
	  ON_PlaneSurface* south_side = 0;
	  ON_PlaneSurface* north_side = 0;
	  srf.Split( dir, srf.Domain(dir).Mid() south_side, north_side );

  */
  BOOL Split(
	 int dir,
	 double c,
	 ON_Surface*& west_or_south_side,
	 ON_Surface*& east_or_north_side
	 ) const;

  /*
  Description:
    Get the parameters of the point on the surface that is closest to P.
  Parameters:
    P - [in] 
	    test point
    s - [out]
    t - [out] 
	    (*s,*t) = parameters of the surface point that 
	    is closest to P.
    maximum_distance = 0.0 - [in] 
	    optional upper bound on the distance from P to 
	    the surface.  If you are only interested in 
	    finding a point Q on the surface when 
	    P.DistanceTo(Q) < maximum_distance, then set
	    maximum_distance to that value.
    sdomain = 0 - [in] optional domain restriction
    tdomain = 0 - [in] optional domain restriction
  Returns:
    True if successful.  If false, the values of *s and *t
    are undefined.
  See Also:
    ON_Surface::GetLocalClosestPoint.
  */
  bool GetClosestPoint( 
	  const ON_3dPoint& P,
	  double* s,
	  double* t,
	  double maximum_distance = 0.0,
	  const ON_Interval* sdomain = 0,
	  const ON_Interval* tdomain = 0
	  ) const;

  //////////
  // Find parameters of the point on a surface that is locally closest to 
  // the test_point.  The search for a local close point starts at 
  // seed parameters. If a sub_domain parameter is not NULL, then
  // the search is restricted to the specified portion of the surface.
  //
  // TRUE if returned if the search is successful.  FALSE is returned if
  // the search fails.
  BOOL GetLocalClosestPoint( const ON_3dPoint&, // test_point
	  double,double,     // seed_parameters
	  double*,double*,   // parameters of local closest point returned here
	  const ON_Interval* = NULL, // first parameter sub_domain
	  const ON_Interval* = NULL  // second parameter sub_domain
	  ) const;


  /*
  Description:
    Offset surface.
  Parameters:
    offset_distance - [in] offset distance
    tolerance - [in] Some surfaces do not have an exact offset that
      can be represented using the same class of surface definition.
      In that case, the tolerance specifies the desired accuracy.
    max_deviation - [out] If this parameter is not NULL, the maximum
      deviation from the returned offset to the TRUE offset is returned
      here.  This deviation is zero except for cases where an exact
      offset cannot be computed using the same class of surface definition.
  Remarks:
    Overrides virtual ON_Surface::Offset.
  Returns:
    Offset surface.
  */
  ON_Surface* Offset(
	double offset_distance, 
	double tolerance, 
	double* max_deviation = NULL
	) const;


  int GetNurbForm( // returns 0: unable to create NURBS representation
		   //            with desired accuracy.
		   //         1: success - returned NURBS parameterization
		   //            matches the surface's to wthe desired accuracy
		   //         2: success - returned NURBS point locus matches
		   //            the surfaces's to the desired accuracy but, on
		   //            the interior of the surface's domain, the 
		   //            surface's parameterization and the NURBS
		   //            parameterization may not match to the 
		   //            desired accuracy.
	ON_NurbsSurface&,
	double = 0.0
	) const;

  int HasNurbForm( // returns 0: unable to create NURBS representation
		   //            with desired accuracy.
		   //         1: success - returned NURBS parameterization
		   //            matches the surface's to wthe desired accuracy
		   //         2: success - returned NURBS point locus matches
		   //            the surfaces's to the desired accuracy but, on
		   //            the interior of the surface's domain, the 
		   //            surface's parameterization and the NURBS
		   //            parameterization may not match to the 
		   //            desired accuracy.
	) const;

  /*
  Description:
    Sets the extents of then rectangle.  Does not change the evaluation
    domain.
  Parameters:
    dir - [in] 0 sets plane's x coordinate extents
	       0 sets plane's y coordinate extents
    extents - [in] increasing interval
    bSynchDomain - [in] if true, the corresponding evaluation interval
	       domain is set so that it matches the extents interval
  Returns:
    True if successful.
  See Also:
    ON_PlaneSurface::SetDomain
  */
  bool SetExtents( 
	 int dir,
	 ON_Interval extents,
	 bool bSynchDomain = false
	 );

  /*
  Description:
    Gets the extents of the rectangle.
  Parameters:
    dir - [in] 0 gets plane's x coordinate extents
	       0 gets plane's y coordinate extents
  Returns:
    Increasing interval
  See Also:
    ON_PlaneSurface::Domain
  */
  ON_Interval Extents(
	 int dir
	 ) const;

public:
  static ON_MeshPlaneSurface _MeshPlaneSurface;

protected:
  // evaluation domain (always increasing)
  ON_Interval m_domain[2]; // always increasing

  // rectangle extents (in m_plane x,y coordinates)
  ON_Interval m_extents[2];
};


class ON_CLASS ON_ClippingPlaneSurface : public ON_PlaneSurface
{
  ON_OBJECT_DECLARE(ON_ClippingPlaneSurface);
public:
  ON_ClippingPlaneSurface();
  ON_ClippingPlaneSurface(const ON_Plane& src);
  ON_ClippingPlaneSurface(const ON_PlaneSurface& src);
  ~ON_ClippingPlaneSurface();

  ON_ClippingPlaneSurface& operator=(const ON_Plane& src);
  ON_ClippingPlaneSurface& operator=(const ON_PlaneSurface& src);

  void Default();

  // override ON_Object::ObjectType() - returns ON::clipplane_object
  ON::object_type ObjectType() const;

  // virtual ON_Object::SizeOf override
  unsigned int SizeOf() const;

  // virtual ON_Object::DataCRC override
  ON__UINT32 DataCRC(ON__UINT32 current_remainder) const;

  // virtual ON_Object::Dump override
  void Dump( ON_TextLog& ) const; // for debugging

  // virtual ON_Object::Write override
  BOOL Write(
	 ON_BinaryArchive&  // open binary file
       ) const;

  // virtual ON_Object::Read override
  BOOL Read(
	 ON_BinaryArchive&  // open binary file
       );

  ON_ClippingPlane m_clipping_plane;
};


#endif
