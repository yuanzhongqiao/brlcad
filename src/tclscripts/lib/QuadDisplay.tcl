#                 Q U A D D I S P L A Y . T C L
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Copyright Notice -
#       This software is Copyright (C) 1998-2004 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	The QuadDisplay class is comprised of four Display objects. This class
#       keeps track of the current Display object and provides the means to toggle
#       between showing all four Display objects or just the current one.
#

option add *Pane*margin 0 widgetDefault
option add *QuadDisplay.width 400 widgetDefault
option add *QuadDisplay.height 400 widgetDefault

class QuadDisplay {
    inherit iwidgets::Panedwindow

    itk_option define -pane pane Pane ur
    itk_option define -multi_pane multi_pane Multi_pane 0
    itk_option define -paneCallback paneCallback PaneCallback ""

    constructor {args} {}
    destructor {}

    public method pane {args}
    public method multi_pane {args}
    public method refresh {}
    public method refreshAll {}

    # methods for controlling the view object
    public method ae {args}
    public method arot {args}
    public method base2local {}
    public method center {args}
    public method coord {args}
    public method eye {args}
    public method eye_pos {args}
    public method invSize {args}
    public method keypoint {args}
    public method local2base {}
    public method lookat {args}
    public method model2view {args}
    public method mrot {args}
    public method orientation {args}
    public method pmat {args}
    public method pmodel2view {args}
    public method pov {args}
    public method rmat {args}
    public method rot {args}
    public method rotate_about {args}
    public method sca {args}
    public method setview {args}
    public method size {args}
    public method slew {args}
    public method tra {args}
    public method units {args}
    public method view2model {args}
    public method vrot {args}
    public method vtra {args}
    public method zoom {args}

    public method autoview {{gindex 0}}
    public method autoviewAll {{gindex 0}}

    public method add {glist}
    public method addAll {glist}
    public method remove {glist}
    public method removeAll {glist}
    public method contents {}

    public method bg {args}
    public method bgAll {args}
    public method bounds {args}
    public method boundsAll {args}
    public method fb_active {args}
    public method fb_observe {args}
    public method depthMask {args}
    public method light {args}
    public method linestyle {args}
    public method linewidth {args}
    public method listen {args}
    public method perspective {args}
    public method perspective_angle {args}
    public method sync {}
    public method png {args}
    public method mouse_nirt {x y}
    public method nirt {args}
    public method vnirt {vx vy}
    public method qray {args}
    public method rt {args}
    public method rtabort {{gi 0}}
    public method rtcheck {args}
    public method rtedge {args}
    public method transparency {args}
    public method zbuffer {args}
    public method zclip {args}

    public method toggle_modelAxesEnable {args}
    public method toggle_modelAxesEnableAll {}
    public method toggle_modelAxesTickEnable {args}
    public method toggle_modelAxesTickEnableAll {}
    public method toggle_viewAxesEnable {args}
    public method toggle_viewAxesEnableAll {}
    public method toggle_centerDotEnable {args}
    public method toggle_centerDotEnableAll {}

    public method resetAll {}
    public method default_views {}
    public method attach_view {}
    public method attach_viewAll {}
    public method attach_drawable {dg}
    public method attach_drawableAll {dg}
    public method detach_view {}
    public method detach_viewAll {}
    public method detach_drawable {dg}
    public method detach_drawableAll {dg}

    public method lightAll {args}
    public method transparencyAll {args}
    public method zbufferAll {args}
    public method zclipAll {args}

    public method setCenterDotEnable {args}
    public method setViewAxesEnable {args}
    public method setModelAxesEnable {args}
    public method setViewAxesPosition {args}
    public method setModelAxesPosition {args}
    public method setModelAxesTickEnable {args}
    public method setModelAxesTickInterval {args}
    public method setModelAxesTicksPerMajor {args}

    public method rotate_mode {x y}
    public method translate_mode {x y}
    public method scale_mode {x y}

    public method resetBindings {}
    public method resetBindingsAll {}

    public method ? {}
    public method apropos {key}
    public method getUserCmds {}
    public method help {args}

    protected method toggle_multi_pane {}

    private variable priv_pane ur
    private variable priv_multi_pane 1
}

body QuadDisplay::constructor {args} {
    iwidgets::Panedwindow::add upper
    iwidgets::Panedwindow::add lower

    # create two more panedwindows
    itk_component add upw {
	::iwidgets::Panedwindow [childsite upper].pw -orient vertical
    } {
	usual
	keep -sashwidth -sashheight -sashborderwidth
	keep -sashindent -thickness
    }

    itk_component add lpw {
	::iwidgets::Panedwindow [childsite lower].pw -orient vertical
    } {
	usual
	keep -sashwidth -sashheight -sashborderwidth
	keep -sashindent -thickness
    }

    $itk_component(upw) add ulp
    $itk_component(upw) add urp
    $itk_component(lpw) add llp
    $itk_component(lpw) add lrp

    # create four instances of Display
    itk_component add ul {
	Display [$itk_component(upw) childsite ulp].display
    } {
	usual
    }

    itk_component add ur {
	Display [$itk_component(upw) childsite urp].display
    } {
	usual
    }

    itk_component add ll {
	Display [$itk_component(lpw) childsite llp].display
    } {
	usual
    }

    itk_component add lr {
	Display [$itk_component(lpw) childsite lrp].display
    } {
	usual
    }

    # initialize the views
    default_views

    pack $itk_component(ul) -fill both -expand yes
    pack $itk_component(ur) -fill both -expand yes
    pack $itk_component(ll) -fill both -expand yes
    pack $itk_component(lr) -fill both -expand yes

    pack $itk_component(upw) -fill both -expand yes
    pack $itk_component(lpw) -fill both -expand yes

    catch {eval itk_initialize $args}
}

configbody QuadDisplay::pane {
    pane $itk_option(-pane)
}

configbody QuadDisplay::multi_pane {
    multi_pane $itk_option(-multi_pane)
}

body QuadDisplay::pane {args} {
    # get the active pane
    if {$args == ""} {
	return $itk_option(-pane)
    }

    # set the active pane
    switch $args {
	ul -
	ur -
	ll -
	lr {
	    set itk_option(-pane) $args
	}
	default {
	    return -code error "pane: bad value - $args"
	}
    }

    if {!$itk_option(-multi_pane)} {
	# nothing to do
	if {$priv_pane == $itk_option(-pane)} {
	    return
	}

	switch $priv_pane {
	    ul {
		switch $itk_option(-pane) {
		    ur {
			$itk_component(upw) hide ulp
			$itk_component(upw) show urp
		    }
		    ll {
			iwidgets::Panedwindow::hide upper
			$itk_component(upw) show urp
			iwidgets::Panedwindow::show lower
			$itk_component(lpw) show llp
			$itk_component(lpw) hide lrp
		    }
		    lr {
			iwidgets::Panedwindow::hide upper
			$itk_component(upw) show urp
			iwidgets::Panedwindow::show lower
			$itk_component(lpw) hide llp
			$itk_component(lpw) show lrp
		    }
		}
	    }
	    ur {
		switch $itk_option(-pane) {
		    ul {
			$itk_component(upw) hide urp
			$itk_component(upw) show ulp
		    }
		    ll {
			iwidgets::Panedwindow::hide upper
			$itk_component(upw) show ulp
			iwidgets::Panedwindow::show lower
			$itk_component(lpw) show llp
			$itk_component(lpw) hide lrp
		    }
		    lr {
			iwidgets::Panedwindow::hide upper
			$itk_component(upw) show ulp
			iwidgets::Panedwindow::show lower
			$itk_component(lpw) hide llp
			$itk_component(lpw) show lrp
		    }
		}
	    }
	    ll {
		switch $itk_option(-pane) {
		    ul {
			iwidgets::Panedwindow::hide lower
			$itk_component(lpw) show lrp
			iwidgets::Panedwindow::show upper
			$itk_component(upw) hide urp
			$itk_component(upw) show ulp
		    }
		    ur {
			iwidgets::Panedwindow::hide lower
			$itk_component(lpw) show lrp
			iwidgets::Panedwindow::show upper
			$itk_component(upw) hide ulp
			$itk_component(upw) show urp
		    }
		    lr {
			$itk_component(lpw) hide llp
			$itk_component(lpw) show lrp
		    }
		}
	    }
	    lr {
		switch $itk_option(-pane) {
		    ul {
			iwidgets::Panedwindow::hide lower
			$itk_component(lpw) show llp
			iwidgets::Panedwindow::show upper
			$itk_component(upw) hide urp
			$itk_component(upw) show ulp
		    }
		    ur {
			iwidgets::Panedwindow::hide lower
			$itk_component(lpw) show llp
			iwidgets::Panedwindow::show upper
			$itk_component(upw) hide ulp
			$itk_component(upw) show urp
		    }
		    ll {
			$itk_component(lpw) hide lrp
			$itk_component(lpw) show llp
		    }
		}
	    }
	}
    }

	set priv_pane $itk_option(-pane)

    if {$itk_option(-paneCallback) != ""} {
	catch {eval $itk_option(-paneCallback) $args}
    }
}

body QuadDisplay::multi_pane {args} {
    # get multi_pane
    if {$args == ""} {
	return $itk_option(-multi_pane)
    }

    # nothing to do
    if {$args == $priv_multi_pane} {
	return
    }

    switch $args {
	0 -
	1 {
	    toggle_multi_pane
	}
	default {
	    return -code error "mult_pane: bad value - $args"
	}
    }
}

body QuadDisplay::refresh {} {
    $itk_component($itk_option(-pane)) refresh
}

body QuadDisplay::refreshAll {} {
    $itk_component(ul) refresh
    $itk_component(ur) refresh
    $itk_component(ll) refresh
    $itk_component(lr) refresh
}

body QuadDisplay::ae {args} {
    eval $itk_component($itk_option(-pane)) ae $args
}

body QuadDisplay::arot {args} {
    eval $itk_component($itk_option(-pane)) arot $args
}

body QuadDisplay::base2local {} {
    $itk_component($itk_option(-pane)) base2local
}

body QuadDisplay::center {args} {
    eval $itk_component($itk_option(-pane)) center $args
}

body QuadDisplay::coord {args} {
    eval $itk_component($itk_option(-pane)) coord $args
}

body QuadDisplay::eye {args} {
    eval $itk_component($itk_option(-pane)) eye $args
}

body QuadDisplay::eye_pos {args} {
    eval $itk_component($itk_option(-pane)) eye_pos $args
}

body QuadDisplay::invSize {args} {
    eval $itk_component($itk_option(-pane)) invSize $args
}

body QuadDisplay::keypoint {args} {
    eval $itk_component($itk_option(-pane)) keypoint $args
}

body QuadDisplay::local2base {} {
    $itk_component($itk_option(-pane)) local2base
}

body QuadDisplay::lookat {args} {
    eval $itk_component($itk_option(-pane)) lookat $args
}

body QuadDisplay::model2view {args} {
    eval $itk_component($itk_option(-pane)) model2view $args
}

body QuadDisplay::mrot {args} {
    eval $itk_component($itk_option(-pane)) mrot $args
}

body QuadDisplay::orientation {args} {
    eval $itk_component($itk_option(-pane)) orientation $args
}

body QuadDisplay::pmat {args} {
    eval $itk_component($itk_option(-pane)) pmat $args
}

body QuadDisplay::pmodel2view {args} {
    eval $itk_component($itk_option(-pane)) pmodel2view $args
}

body QuadDisplay::pov {args} {
    eval $itk_component($itk_option(-pane)) pov $args
}

body QuadDisplay::rmat {args} {
    eval $itk_component($itk_option(-pane)) rmat $args
}

body QuadDisplay::rot {args} {
    eval $itk_component($itk_option(-pane)) rot $args
}

body QuadDisplay::rotate_about {args} {
    eval $itk_component($itk_option(-pane)) rotate_about $args
}

body QuadDisplay::slew {args} {
    eval $itk_component($itk_option(-pane)) slew $args
}

body QuadDisplay::sca {args} {
    eval $itk_component($itk_option(-pane)) sca $args
}

body QuadDisplay::setview {args} {
    eval $itk_component($itk_option(-pane)) setview $args
}

body QuadDisplay::size {args} {
    eval $itk_component($itk_option(-pane)) size $args
}

body QuadDisplay::tra {args} {
    eval $itk_component($itk_option(-pane)) tra $args
}

body QuadDisplay::units {args} {
    eval $itk_component(ul) units $args
    eval $itk_component(ur) units $args
    eval $itk_component(ll) units $args
    eval $itk_component(lr) units $args
}

body QuadDisplay::view2model {args} {
    eval $itk_component($itk_option(-pane)) view2model $args
}

body QuadDisplay::vrot {args} {
    eval $itk_component($itk_option(-pane)) vrot $args
}

body QuadDisplay::vtra {args} {
    eval $itk_component($itk_option(-pane)) vtra $args
}

body QuadDisplay::zoom {args} {
    eval $itk_component($itk_option(-pane)) zoom $args
}

body QuadDisplay::autoview {{gindex 0}} {
    $itk_component($itk_option(-pane)) autoview $gindex
}

body QuadDisplay::autoviewAll {{gindex 0}} {
    $itk_component(ul) autoview $gindex
    $itk_component(ur) autoview $gindex
    $itk_component(ll) autoview $gindex
    $itk_component(lr) autoview $gindex
}

body QuadDisplay::add {glist} {
    $itk_component($itk_option(-pane)) add $glist
}

body QuadDisplay::addAll {glist} {
    $itk_component(ul) add $glist
    $itk_component(ur) add $glist
    $itk_component(ll) add $glist
    $itk_component(lr) add $glist
}

body QuadDisplay::remove {glist} {
    $itk_component($itk_option(-pane)) remove $glist
}

body QuadDisplay::removeAll {glist} {
    $itk_component(ul) remove $glist
    $itk_component(ur) remove $glist
    $itk_component(ll) remove $glist
    $itk_component(lr) remove $glist
}

body QuadDisplay::contents {} {
    $itk_component($itk_option(-pane)) contents
}

body QuadDisplay::listen {args} {
    eval $itk_component($itk_option(-pane)) listen $args
}

body QuadDisplay::linewidth {args} {
    set result [eval $itk_component($itk_option(-pane)) linewidth $args]
    if {$args != ""} {
	refresh
	return
    }
    return $result
}

body QuadDisplay::linestyle {args} {
    eval $itk_component($itk_option(-pane)) linestyle $args
}

body QuadDisplay::zclip {args} {
    eval $itk_component($itk_option(-pane)) zclip $args
}

body QuadDisplay::toggle_modelAxesEnable {args} {
    switch -- $args {
	ul -
	ur -
	ll -
	lr {
	    eval $itk_component($args) toggle_modelAxesEnable
	}
	default {
	    eval $itk_component($itk_option(-pane)) toggle_modelAxesEnable
	}
    }
}

body QuadDisplay::toggle_modelAxesEnableAll {} {
    eval $itk_component(ul) toggle_modelAxesEnable
    eval $itk_component(ur) toggle_modelAxesEnable
    eval $itk_component(ll) toggle_modelAxesEnable
    eval $itk_component(lr) toggle_modelAxesEnable
}

body QuadDisplay::toggle_modelAxesTickEnable {args} {
    switch -- $args {
	ul -
	ur -
	ll -
	lr {
	    eval $itk_component($args) toggle_modelAxesTickEnable
	}
	default {
	    eval $itk_component($itk_option(-pane)) toggle_modelAxesTickEnable
	}
    }
}

body QuadDisplay::toggle_modelAxesTickEnableAll {} {
    eval $itk_component(ul) toggle_modelAxesTickEnable
    eval $itk_component(ur) toggle_modelAxesTickEnable
    eval $itk_component(ll) toggle_modelAxesTickEnable
    eval $itk_component(lr) toggle_modelAxesTickEnable
}

body QuadDisplay::toggle_viewAxesEnable {args} {
    switch -- $args {
	ul -
	ur -
	ll -
	lr {
	    eval $itk_component($args) toggle_viewAxesEnable
	}
	default {
	    eval $itk_component($itk_option(-pane)) toggle_viewAxesEnable
	}
    }
}

body QuadDisplay::toggle_viewAxesEnableAll {} {
    eval $itk_component(ul) toggle_viewAxesEnable
    eval $itk_component(ur) toggle_viewAxesEnable
    eval $itk_component(ll) toggle_viewAxesEnable
    eval $itk_component(lr) toggle_viewAxesEnable
}

body QuadDisplay::toggle_centerDotEnable {args} {
    switch -- $args {
	ul -
	ur -
	ll -
	lr {
	    eval $itk_component($args) toggle_centerDotEnable
	}
	default {
	    eval $itk_component($itk_option(-pane)) toggle_centerDotEnable
	}
    }
}

body QuadDisplay::toggle_centerDotEnableAll {} {
    eval $itk_component(ul) toggle_centerDotEnable
    eval $itk_component(ur) toggle_centerDotEnable
    eval $itk_component(ll) toggle_centerDotEnable
    eval $itk_component(lr) toggle_centerDotEnable
}

body QuadDisplay::zclipAll {args} {
    eval $itk_component(ul) zclip $args
    eval $itk_component(ur) zclip $args
    eval $itk_component(ll) zclip $args
    eval $itk_component(lr) zclip $args
}

body QuadDisplay::setCenterDotEnable {args} {
    set ve [eval $itk_component($itk_option(-pane)) configure -centerDotEnable $args]

    # we must be doing a "get"
    if {$ve != ""} {
	return [lindex $ve 4]
    }
}

body QuadDisplay::setViewAxesEnable {args} {
    set ve [eval $itk_component($itk_option(-pane)) configure -viewAxesEnable $args]

    # we must be doing a "get"
    if {$ve != ""} {
	return [lindex $ve 4]
    }
}

body QuadDisplay::setModelAxesEnable {args} {
    set me [eval $itk_component($itk_option(-pane)) configure -modelAxesEnable $args]

    # we must be doing a "get"
    if {$me != ""} {
	return [lindex $me 4]
    }
}

body QuadDisplay::setViewAxesPosition {args} {
    set vap [eval $itk_component($itk_option(-pane)) configure -viewAxesPosition $args]

    # we must be doing a "get"
    if {$vap != ""} {
	return [lindex $vap 4]
    }
}

body QuadDisplay::setModelAxesPosition {args} {
    set map [eval $itk_component($itk_option(-pane)) configure -modelAxesPosition $args]

    # we must be doing a "get"
    if {$map != ""} {
	set mm2local [$itk_component($itk_option(-pane)) base2local]
	set map [lindex $map 4]
	set x [lindex $map 0]
	set y [lindex $map 1]
	set z [lindex $map 2]

	return [list [expr {$x * $mm2local}] \
		     [expr {$y * $mm2local}] \
		     [expr {$z * $mm2local}]]
    }
}

body QuadDisplay::setModelAxesTickEnable {args} {
    set te [eval $itk_component($itk_option(-pane)) configure -modelAxesTickEnable $args]

    # we must be doing a "get"
    if {$te != ""} {
	return [lindex $te 4]
    }
}

body QuadDisplay::setModelAxesTickInterval {args} {
    set ti [eval $itk_component($itk_option(-pane)) configure -modelAxesTickInterval $args]

    # we must be doing a "get"
    if {$ti != ""} {
	set ti [lindex $ti 4]
	return [expr {$ti * [$itk_component($itk_option(-pane)) base2local]}]
    }
}

body QuadDisplay::setModelAxesTicksPerMajor {args} {
    set tpm [eval $itk_component($itk_option(-pane)) configure -modelAxesTicksPerMajor $args]

    # we must be doing a "get"
    if {$tpm != ""} {
	return [lindex $tpm 4]
    }
}

body QuadDisplay::rotate_mode {x y} {
    eval $itk_component($itk_option(-pane)) rotate_mode $x $y
}

body QuadDisplay::translate_mode {x y} {
    eval $itk_component($itk_option(-pane)) translate_mode $x $y
}

body QuadDisplay::scale_mode {x y} {
    eval $itk_component($itk_option(-pane)) scale_mode $x $y
}

body QuadDisplay::transparency {args} {
    eval $itk_component($itk_option(-pane)) transparency $args
}

body QuadDisplay::transparencyAll {args} {
    eval $itk_component(ul) transparency $args
    eval $itk_component(ur) transparency $args
    eval $itk_component(ll) transparency $args
    eval $itk_component(lr) transparency $args
}

body QuadDisplay::zbuffer {args} {
    eval $itk_component($itk_option(-pane)) zbuffer $args
}

body QuadDisplay::zbufferAll {args} {
    eval $itk_component(ul) zbuffer $args
    eval $itk_component(ur) zbuffer $args
    eval $itk_component(ll) zbuffer $args
    eval $itk_component(lr) zbuffer $args
}

body QuadDisplay::depthMask {args} {
    eval $itk_component($itk_option(-pane)) depthMask $args
}

body QuadDisplay::light {args} {
    eval $itk_component($itk_option(-pane)) light $args
}

body QuadDisplay::lightAll {args} {
    eval $itk_component(ul) light $args
    eval $itk_component(ur) light $args
    eval $itk_component(ll) light $args
    eval $itk_component(lr) light $args
}

body QuadDisplay::perspective {args} {
    eval $itk_component($itk_option(-pane)) perspective $args
}

body QuadDisplay::perspective_angle {args} {
    eval $itk_component($itk_option(-pane)) perspective_angle $args
}

body QuadDisplay::sync {} {
    $itk_component($itk_option(-pane)) sync
}

body QuadDisplay::png {args} {
    eval $itk_component($itk_option(-pane)) png $args
}

body QuadDisplay::bg {args} {
    set result [eval $itk_component($itk_option(-pane)) bg $args]
    if {$args != ""} {
	refresh
	return
    }
    return $result
}

body QuadDisplay::bgAll {args} {
    set result [eval $itk_component(ul) bg $args]
    eval $itk_component(ur) bg $args
    eval $itk_component(ll) bg $args
    eval $itk_component(lr) bg $args

    if {$args != ""} {
	refreshAll
	return
    }

    return $result
}

body QuadDisplay::bounds {args} {
    eval $itk_component($itk_option(-pane)) bounds $args
}

body QuadDisplay::boundsAll {args} {
    eval $itk_component(ul) bounds $args
    eval $itk_component(ur) bounds $args
    eval $itk_component(ll) bounds $args
    eval $itk_component(lr) bounds $args
}

body QuadDisplay::fb_active {args} {
    eval $itk_component($itk_option(-pane)) fb_active $args
}

body QuadDisplay::fb_observe {args} {
    eval $itk_component($itk_option(-pane)) fb_observe $args
}

body QuadDisplay::mouse_nirt {x y} {
    eval $itk_component($itk_option(-pane)) mouse_nirt $x $y
}

body QuadDisplay::nirt {args} {
    eval $itk_component($itk_option(-pane)) nirt $args
}

body QuadDisplay::vnirt {vx vy} {
    eval $itk_component($itk_option(-pane)) vnirt $vx $vy
}

body QuadDisplay::qray {args} {
    eval $itk_component($itk_option(-pane)) qray $args
}

body QuadDisplay::rt {args} {
    eval $itk_component($itk_option(-pane)) rt $args
}

body QuadDisplay::rtabort {{gi 0}} {
    $itk_component($itk_option(-pane)) rtabort $gi
}

body QuadDisplay::rtcheck {args} {
    eval $itk_component($itk_option(-pane)) rtcheck $args
}

body QuadDisplay::rtedge {args} {
    eval $itk_component($itk_option(-pane)) rtedge $args
}

body QuadDisplay::resetAll {} {
    reset
    $itk_component(upw) reset
    $itk_component(lpw) reset
}

body QuadDisplay::default_views {} {
    $itk_component(ul) ae 0 90 0
    $itk_component(ur) ae 35 25 0
    $itk_component(ll) ae 0 0 0
    $itk_component(lr) ae 90 0 0
}

body QuadDisplay::attach_view {} {
    $itk_component($itk_option(-pane)) attach_view
}

body QuadDisplay::attach_viewAll {} {
    $itk_component(ul) attach_view
    $itk_component(ur) attach_view
    $itk_component(ll) attach_view
    $itk_component(lr) attach_view
}

body QuadDisplay::attach_drawable {dg} {
    $itk_component($itk_option(-pane)) attach_drawable $dg
}

body QuadDisplay::attach_drawableAll {dg} {
    $itk_component(ul) attach_drawable $dg
    $itk_component(ur) attach_drawable $dg
    $itk_component(ll) attach_drawable $dg
    $itk_component(lr) attach_drawable $dg
}

body QuadDisplay::detach_view {} {
    $itk_component($itk_option(-pane)) detach_view
}

body QuadDisplay::detach_viewAll {} {
    $itk_component(ul) detach_view
    $itk_component(ur) detach_view
    $itk_component(ll) detach_view
    $itk_component(lr) detach_view
}

body QuadDisplay::detach_drawable {dg} {
    $itk_component($itk_option(-pane)) detach_drawable $dg
}

body QuadDisplay::detach_drawableAll {dg} {
    $itk_component(ul) detach_drawable $dg
    $itk_component(ur) detach_drawable $dg
    $itk_component(ll) detach_drawable $dg
    $itk_component(lr) detach_drawable $dg
}

body QuadDisplay::toggle_multi_pane {} {
    if {$priv_multi_pane} {
	set itk_option(-multi_pane) 0
	set priv_multi_pane 0

	switch $itk_option(-pane) {
	    ul {
		iwidgets::Panedwindow::hide lower
		$itk_component(upw) hide urp
	    }
	    ur {
		iwidgets::Panedwindow::hide lower
		$itk_component(upw) hide ulp
	    }
	    ll {
		iwidgets::Panedwindow::hide upper
		$itk_component(lpw) hide lrp
	    }
	    lr {
		iwidgets::Panedwindow::hide upper
		$itk_component(lpw) hide llp
	    }
	}
    } else {
	set itk_option(-multi_pane) 1
	set priv_multi_pane 1

	switch $itk_option(-pane) {
	    ul {
		iwidgets::Panedwindow::show lower
		$itk_component(upw) show urp
	    }
	    ur {
		iwidgets::Panedwindow::show lower
		$itk_component(upw) show ulp
	    }
	    ll {
		iwidgets::Panedwindow::show upper
		$itk_component(lpw) show lrp
	    }
	    lr {
		iwidgets::Panedwindow::show upper
		$itk_component(lpw) show llp
	    }
	}
    }
}

body QuadDisplay::resetBindings {} {
    $itk_component($itk_option(-pane)) resetBindings
}

body QuadDisplay::resetBindingsAll {} {
    $itk_component(ul) resetBindings
    $itk_component(ur) resetBindings
    $itk_component(ll) resetBindings
    $itk_component(lr) resetBindings
}

body QuadDisplay::? {} {
    return [$itk_component(ur) ?]
}

body QuadDisplay::apropos {key} {
    return [$itk_component(ur) apropos $key]
}

body QuadDisplay::getUserCmds {} {
    return [$itk_component(ur) getUserCmds]
}

body QuadDisplay::help {args} {
    return [eval $itk_component(ur) help $args]
}
