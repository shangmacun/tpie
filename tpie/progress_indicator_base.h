// -*- mode: c++; tab-width: 4; indent-tabs-mode: t; eval: (progn (c-set-style "stroustrup") (c-set-offset 'innamespace 0)); -*-
// vi:set ts=4 sts=4 sw=4 noet :
// Copyright 2008, The TPIE development team
// 
// This file is part of TPIE.
// 
// TPIE is free software: you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.
// 
// TPIE is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
// License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with TPIE.  If not, see <http://www.gnu.org/licenses/>

#ifndef _TPIE_PROGRESS_INDICATOR_BASE_H
#define _TPIE_PROGRESS_INDICATOR_BASE_H

#include <tpie/portability.h>
#include <algorithm>
#include <boost/thread.hpp>
#include <tpie/imported/cycle.h>
#include <tpie/execution_time_predictor.h>
#include <tpie/tpie_log.h>

namespace tpie {

///////////////////////////////////////////////////////////////////
///
///  The base class for indicating the progress of some task.
///
/// At times, especially when processing large data sets, the user might want
/// the program to provide information about how much progress has been made. 
/// TPIE provides a class hierarchy with an abstract base class 
/// \ref progress_indicator_base for realizing such indicators. TPIE offers 
/// terminal-based indicators, such as progress_indicator_arrow that shows an 
/// extending arrow or progress_indicator_spin that shows a spinning "wheel". 
/// To allow for other types of indicators such as graphics-based indicators
/// for (interfaces to) indicators provided by other APIs, the terminal-based
/// indicators inherit from progress_indicator_terminal which in turn inherits
/// from progress_indicator_base. To include other types of non-terminal-based
/// indicators, the user thus should subclass progress_indicator_base. All
/// indicators are based upon the following concept: The indicator is given
/// a range [minRange , maxRange ] and a parameter stepValue/ 
/// For each update to the indicator and starting at minRange , the progress 
/// status will be advanced by stepValue units. 
///////////////////////////////////////////////////////////////////

class progress_indicator_base {
public:
	
	////////////////////////////////////////////////////////////////////
	///
	///  Initializes the indicator. There is a sanity check that 
	///  ensures that minRange <= maxRange and that stepValue 
	///  is in [1,maxRange-minRange].
	///
	///  \param  title        The title of the progress indicator.
	///  \param  description  A text to be printed in front of the 
	///                       indicator.
	///  \param  minRange     The lower bound of the counting range.
	///  \param  maxRange     The upper bound of the counting range.
	///  \param  stepValue    The increment for each step.
	///
	////////////////////////////////////////////////////////////////////
	
	progress_indicator_base(const std::string& /* title */, 
							const std::string& /* description */, 
							TPIE_OS_OFFSET minRange, 
							TPIE_OS_OFFSET maxRange, 
							TPIE_OS_OFFSET stepValue) : 
	    m_minRange(std::min(minRange, maxRange)),
	    m_maxRange(std::max(minRange, maxRange)),
	    m_stepValue(std::max(std::min(stepValue, (m_maxRange-m_minRange)), 
			    static_cast<TPIE_OS_OFFSET>(1))),
	    m_current(0),
	    m_percentageChecker(0), 
	    m_percentageValue(0), 
	    m_percentageUnit(0),
		m_lastUpdate(getticks()),
		m_predictor(0) {
		compute_threshold();
	}

	////////////////////////////////////////////////////////////////////
	///  Copy-constructor.
	////////////////////////////////////////////////////////////////////
	progress_indicator_base(const progress_indicator_base& other) :
	    m_minRange(other.m_minRange),
	    m_maxRange(other.m_maxRange),
	    m_stepValue(other.m_stepValue),
	    m_current(other.m_current),
	    m_percentageChecker(other.m_percentageChecker),
	    m_percentageValue(other.m_percentageValue),
	    m_percentageUnit(other.m_percentageUnit),
		m_lastUpdate(other.m_lastUpdate),
		m_predictor(other.m_predictor) {
				compute_threshold();
	    }

	////////////////////////////////////////////////////////////////////
	///  Assignment operator.
	////////////////////////////////////////////////////////////////////
	progress_indicator_base& operator=(const progress_indicator_base& other) {
	    if (this != &other) {
		m_percentageChecker = other.m_percentageChecker;
		m_percentageValue   = other.m_percentageValue;
		m_percentageUnit    = other.m_percentageUnit;
		m_maxRange          = other.m_maxRange;
		m_minRange          = other.m_minRange;
		m_stepValue         = other.m_stepValue;
		m_current           = other.m_current;
		m_lastUpdate        = other.m_lastUpdate;
	    }
	    return *this;
	}

	////////////////////////////////////////////////////////////////////
	///  The destructor. Nothing is done.
	////////////////////////////////////////////////////////////////////
	virtual ~progress_indicator_base() {
	    // Do nothing.
	};
    
	////////////////////////////////////////////////////////////////////
	///
	///  Simultaneously set the upper and lower bound of the counting
	///  range. Also, set the increment for each step. There is a sanity 
	///  check that ensures that minRange <= maxRange and that stepValue 
	///  is in [1,maxRange-minRange].
	///
	///  \param  minRange     The lower bound of the counting range.
	///  \param  maxRange     The upper bound of the counting range.
	///  \param  stepValue    The increment for each step.
	///
	////////////////////////////////////////////////////////////////////
	void set_range(TPIE_OS_OFFSET minRange, TPIE_OS_OFFSET maxRange, TPIE_OS_OFFSET stepValue) {
	    set_min_range(std::min(minRange, maxRange));
	    set_max_range(std::max(minRange, maxRange));
	    set_step_value( std::max(std::min(stepValue, (m_maxRange-m_minRange)),
				static_cast<TPIE_OS_OFFSET>(1)));
	    m_percentageValue = 0;
	    m_percentageChecker = 0;
	    m_percentageUnit = 0;
	    reset();
	}

	void set_range(TPIE_OS_OFFSET range, TPIE_OS_OFFSET step=1) {
		set_range(0, range, step);
	}
    
	////////////////////////////////////////////////////////////////////
	///  Simultaneously set the upper and lower bound of the counting
	///  range and set the increment to be max(1,0.01(maxRange-minRange)).
	///  There is a sanity check that ensures that minRange <= maxRange.
	///
	///  \param  minRange        The lower bound of the counting range.
	///  \param  maxRange        The upper bound of the counting range.
	///  \param  percentageUnit  1/percentageUnit is one "percent".
	////////////////////////////////////////////////////////////////////
	void set_percentage_range(TPIE_OS_OFFSET minRange, TPIE_OS_OFFSET maxRange, unsigned short percentageUnit = 100) {
	    TPIE_OS_OFFSET localMin = std::min(minRange,maxRange);
	    TPIE_OS_OFFSET localMax = std::max(minRange,maxRange);
	    set_step_value(1);
	    m_percentageUnit  = std::max(percentageUnit, 
				    static_cast<unsigned short>(1));
	    m_percentageValue = (localMax-localMin)/m_percentageUnit;
	    if (m_percentageValue > 0) {
		set_min_range(0);
		set_max_range(m_percentageUnit);
	    }
	    else {
		set_min_range(localMin);
		set_max_range(localMax);
		m_percentageValue = 1;
		m_percentageUnit = static_cast<unsigned short>(localMax-localMin);
	    }
	    m_percentageChecker = 0;
	    reset();
	}

	////////////////////////////////////////////////////////////////////
	///  Record an increment but only advance the indicator if it will
	///  be advance by at least one percent.
	////////////////////////////////////////////////////////////////////
	void step_percentage() {
	    //  Increase the step counter.
	    ++m_percentageChecker;
	    m_percentageChecker = m_percentageChecker % m_percentageValue;

	    //  If the number of steps since the last update is large
	    //  enough to constiture one "percent", advance the indicator.
	    if ((!m_percentageChecker) && (m_current < m_maxRange)) 
		step();
	}

	////////////////////////////////////////////////////////////////////
	///  Record an increment to the indicator and advance the indicator.
	////////////////////////////////////////////////////////////////////
	void step(TPIE_OS_OFFSET step) {
	    m_current += step;
		ticks currentTicks = getticks();
#ifndef NDEBUG
		if ( elapsed(currentTicks,m_lastUpdate) > m_frequency * m_threshold * 5) {
			TP_LOG_WARNING("Step was not called for 5 seconds\n");
		}
#endif	
		if(elapsed(currentTicks, m_lastUpdate) > m_threshold){
			m_lastUpdate = currentTicks;
		    refresh();
		}
	}

	void step() {step(m_stepValue);}

	virtual void init(TPIE_OS_OFFSET range, TPIE_OS_OFFSET step=1) {
		if (range != 0) set_range(0, range, step);
	    m_current = m_minRange;
		m_lastUpdate = getticks();
	    refresh();
	}

	////////////////////////////////////////////////////////////////////
	///  Display a zero count. This method may also be used to 
	///  simultaneously set a new description.
	////////////////////////////////////////////////////////////////////
	void init(const std::string& description = std::string()) {
		unused(description);
		init(0);
	}

	////////////////////////////////////////////////////////////////////
	///  Reset the counter. The current position is reset to the
	///  lower bound of the counting range.
	////////////////////////////////////////////////////////////////////
	virtual void reset() {
	    m_current = m_minRange;
	}

	////////////////////////////////////////////////////////////////////
	///
	///  Advance the indicator to the end and print an (optional)
	///  message that is followed by a newline.
	///
	///  \param  text  The message to be printed at the end of the
	///                indicator.
	///
	////////////////////////////////////////////////////////////////////

	virtual void done() {}
	void done(const std::string& text) {unused(text); done();}
	
	////////////////////////////////////////////////////////////////////
	///
	///  Set the lower bound of the counting range. This method
	///  also implies a reset of the counter. In order to be able
	///  to set the lower bound independent of setting the upper bound,
	///  no range checking is done.
	///
	///  \param  minRange  The new lower bound.
	///
	////////////////////////////////////////////////////////////////////
	virtual void set_min_range(TPIE_OS_OFFSET minRange) {
	    m_minRange = minRange;
	    reset();
	}

		////////////////////////////////////////////////////////////////////
	///
	///  Set the upper bound of the counting range. This method
	///  also implies a reset of the counter. In order to be able
	///  to set the uper bound independent of setting the lower bound,
	///  no range checking is done.
	///
	///  \param  maxRange  The new upper bound.
	///
	////////////////////////////////////////////////////////////////////
	virtual void set_max_range(TPIE_OS_OFFSET maxRange) {
	    m_maxRange = maxRange;
	    reset();
	}

	////////////////////////////////////////////////////////////////////
	///
	///  Set the increment by which the counter is advanced upon each
	///  call to step(). In order to be able to reset the counter,
	///  no range checking is done.
	///
	///  \param  stepValue  The incerement.
	///
	////////////////////////////////////////////////////////////////////
	virtual void set_step_value(TPIE_OS_OFFSET stepValue) {
	    m_stepValue = stepValue;
	}
  
	////////////////////////////////////////////////////////////////////
	///
	///  Set the title of a new task to be monitored. The terminal
	///  line will be newline'd, and the title will be followed by a
	///  newline as well.
	///
	///  \param  title  The title of the new task to be monitored.
	///
	////////////////////////////////////////////////////////////////////

	void set_title(const std::string&) {}

	////////////////////////////////////////////////////////////////////
	///
	///  Set the description of the task currently being monitored.
	///  Invoking this method will clear the terminal line.
	///
	///  \param  description  The decription of the task being monitored.
	///
	////////////////////////////////////////////////////////////////////

	void set_description(const std::string&) {}

	////////////////////////////////////////////////////////////////////
	///
	///  Display the indicator.
	///
	////////////////////////////////////////////////////////////////////

	virtual void refresh() = 0;

	////////////////////////////////////////////////////////////////////
	///  Get the current value of the step counter.
	////////////////////////////////////////////////////////////////////
	
	TPIE_OS_OFFSET get_current() { return m_current; }
	
	////////////////////////////////////////////////////////////////////
	///  Get the minimum value of the current range.
	////////////////////////////////////////////////////////////////////
	
	TPIE_OS_OFFSET get_min_range() { return m_minRange; }
	
	////////////////////////////////////////////////////////////////////
	///  Get the maximum value of the current range.
	////////////////////////////////////////////////////////////////////

	TPIE_OS_OFFSET get_max_range() { return m_maxRange; }
	
	////////////////////////////////////////////////////////////////////
	///  Get the current number of units the step counter is increased
	///  by each call to step.
	////////////////////////////////////////////////////////////////////

	TPIE_OS_OFFSET get_step_value() { return m_stepValue; }


	execution_time_predictor * get_time_predictor() {return m_predictor;}
	void set_time_predictor(execution_time_predictor * p) {m_predictor = p;}

	std::string estimated_remaining_time() {
		if (m_maxRange - m_minRange == 0 || m_predictor == 0 || m_current < 0) return "";
		return m_predictor->estimate_remaining_time( double(m_current) / double(m_maxRange - m_minRange) );
	}

	virtual void push_breadcrumb(const char *) {}
	virtual void pop_breadcrumb() {}
protected:

	/**  The lower bound of the counting range.  */
	TPIE_OS_OFFSET m_minRange;

	/**  The upper bound of the counting range.  */
	TPIE_OS_OFFSET m_maxRange;

	/**  The increment for each step.  */
	TPIE_OS_OFFSET m_stepValue;

	/**  The current progress count [m_minRange...m_maxRange].  */
	TPIE_OS_OFFSET m_current;

	/**  A temporary counter in [0...m_percentageValue-1].  */
	TPIE_OS_OFFSET m_percentageChecker;

	/**  The absolute value which constitutes one percent of 
	     the counting range.  */
	TPIE_OS_OFFSET m_percentageValue;

	/**  The unit in which "percentage" is measure. Default is
	     to measure in percent, i.e., the unit is 100. A value
	     other than 0 indicates that the counter is in percentage
	     mode, i.e., it displays percent instead of steps. */
	unsigned short m_percentageUnit;

    private:
	/**  The number of ticks elapsed when refresh was called last */
	ticks m_lastUpdate;

	/**  The approximate frequency of calls to refresh in hz */
	static const unsigned int m_frequency;

	/**  The threshold for elapsed ticks before refresh is called again */
	static double m_threshold;

	/**  Indicates whether or not m_threshold has been computed */
	static bool m_thresholdComputed;

	execution_time_predictor * m_predictor;
	//////////////////////////////////////////////////////////////////////////
	///
	///  Makes sure m_threshold has been set.
	///
	//////////////////////////////////////////////////////////////////////////
	static void compute_threshold();

	progress_indicator_base();
    };

}  //  tpie namespace

#endif // _TPIE_PROGRESS_INDICATOR_BASE
