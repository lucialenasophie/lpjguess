///////////////////////////////////////////////////////////////////////////////////////
/// \file driver.cpp
/// \brief Environmental driver calculation/transformation
///
/// \author Ben Smith
/// $Date: 2021-04-22 18:36:50 +0200 (Do, 22 Apr 2021) $
///
///////////////////////////////////////////////////////////////////////////////////////

// WHAT SHOULD THIS FILE CONTAIN?
// Module source code files should contain, in this order:
//   (1) a "#include" directive naming the framework header file. The framework header
//       file should define all classes used as arguments to functions in the present
//       module. It may also include declarations of global functions, constants and
//       types, accessible throughout the model code;
//   (2) other #includes, including header files for other modules accessed by the
//       present one;
//   (3) type definitions, constants and file scope global variables for use within
//       the present module only;
//   (4) declarations of functions defined in this file, if needed;
//   (5) definitions of all functions. Functions that are to be accessible to other
//       modules or to the calling framework should be declared in the module header
//       file.
//
// PORTING MODULES BETWEEN FRAMEWORKS:
// Modules should be structured so as to be fully portable between models (frameworks).
// When porting between frameworks, the only change required should normally be in the
// "#include" directive referring to the framework header file.

#include "config.h"
#include "driver.h"
#include "blaze.h"
#include "simfire.h"

/// Function for generating random numbers
/** Returns a random floating-point number in the range 0-1.
 *  Uses and updates the parameter 'seed' which may be initialised to any
 *  positive integral value (the same initial value will result in the same sequence
 *  of returned values on subsequent calls to randfrac every time the program is
 *  run)
 */
double randfrac(long& seed) {

	// Reference: Park & Miller 1988 CACM 31: 1192

	const long modulus = 2147483647;
	const double fmodulus = modulus;
	const long multiplier = 16807;
	const long q = 127773;
	const long r = 2836;

	seed = multiplier * (seed % q) - r * (seed / q);
	if (!seed) seed++; // increment seed to 1 in unlikely event of 0 value
	else if (seed < 0) seed += modulus;
	return (double)seed / fmodulus;
}

/// Generates quasi-daily values for a single month, based on monthly means
/**
 *  The generated daily values will conserve the monthly mean.
 *
 *  The daily values are generated by first choosing values for the beginning,
 *  middle and end of the month, and interpolating linearly between them.
 *  The end points will be chosen by taking the surrounding months into
 *  account, and the mid point is then chosen so that we conserve the mean.
 *
 *  Could be used for other interpolations than only monthly to daily,
 *  but comments assume monthly to daily to avoid being too abstract.
 *
 *  \param preceding_mean  Mean value for preceding month
 *  \param this_mean       Mean value for the current month
 *  \param succeeding_mean Mean value for the succeeding month
 *  \param time_steps      Number of days in the current month
 *  \param result          The generated daily values
 *                         (array expected to hold at least time_steps values)
 *
 */
void interp_single_month(double preceding_mean,
                         double this_mean,
                         double succeeding_mean,
                         int time_steps,
                         double* result,
                         double minimum = -std::numeric_limits<double>::max(),
                         double maximum = std::numeric_limits<double>::max()) {

	// The values for the beginning and the end of the month are determined
	// from the average of the two adjacent monthly means
	const double first_value = mean(this_mean, preceding_mean);
	const double last_value = mean(this_mean, succeeding_mean);

	// The mid-point value is computed as offset from the mean, so that the
	// average deviation from the mean of first_value and last_value
	// is compensated for.
	// E.g., if the two values at beginning and end of the month are on average
	// 2 degrees cooler than the monthly mean, the mid-monthly value is
	// determined as monthly mean + 2 degrees, so that the monthly mean is
	// conserved.
	const double average_deviation =
		mean(first_value-this_mean, last_value-this_mean);

	const double middle_value = this_mean-average_deviation;
	const double half_time = time_steps/2.0;

	const double first_slope = (middle_value-first_value)/half_time;
	const double second_slope = (last_value-middle_value)/half_time;

	double sum = 0;
	int i = 0;

	// Interpolate the first half
	for (; i < time_steps/2; ++i) {
		double current_time = i+0.5; // middle of day i
		result[i] = first_value + first_slope*current_time;
		sum += result[i];
	}

	// Special case for dealing with the middle day if time_steps is odd
	if (time_steps%2 == 1) {
		// In this case we can't use the value corresponding to the middle
		// of the day. We'll simply skip it and calculate it based on
		// whatever the other days sum up to.
		++i;
	}

	// Interpolate the other half
	for (; i < time_steps; ++i) {
		double current_time = i+0.5; // middle of day i
		result[i] = middle_value + second_slope*(current_time-half_time);
		sum += result[i];
	}

	if (time_steps%2 == 1) {
		// Go back and set the middle value to whatever is needed to
		// conserve the mean
		result[time_steps/2] = time_steps*this_mean-sum;
	}

	// Go through all values and make sure they're all above the minimum
	double added = 0;
	double sum_above = 0;

	for (int i = 0; i < time_steps; ++i) {
		if (result[i] < minimum) {
			added += minimum - result[i];
			result[i] = minimum;
		}
		else {
			sum_above += result[i] - minimum;
		}
	}

	double fraction_to_remove = sum_above > 0 ? added / sum_above : 0;

	for (int i = 0; i < time_steps; ++i) {
		if (result[i] > minimum) {
			result[i] -= fraction_to_remove * (result[i] - minimum);

			// Needed (only) due to limited precision in floating point arithmetic
			result[i] = max(result[i], minimum);
		}
	}

	// Go through all values and make sure they're all below the maximum
	double removed = 0;
	double sum_below = 0;

	for (int i = 0; i < time_steps; ++i) {
		if (result[i] > maximum) {
			removed += result[i] - maximum;
			result[i] = maximum;
		}
		else {
			sum_below += maximum - result[i];
		}
	}

	double fraction_to_add = sum_below > 0 ? removed / sum_below : 0;

	for (int i = 0; i < time_steps; ++i) {
		if (result[i] < maximum) {
			result[i] += fraction_to_add * (maximum - result[i]);

			// Needed (only) due to limited precision in floating point arithmetic
			result[i] = min(result[i], maximum);
		}
	}
}


/// Climate interpolation from monthly means to quasi-daily values
/** May be called from input/output module to generate daily climate values when
 *  raw data are on monthly basis.
 *
 *  The generated daily values will have the same monthly means as the input.
 *
 *  \param mvals The monthly means
 *  \param dvals The generated daily values
 */
void interp_monthly_means_conserve(const double* mvals, double* dvals,
                                   double minimum, double maximum) {

	Date date;
	int start_of_month = 0;

	for (int m = 0; m < 12; m++) {

		// Index of previous and next month, with wrap-around
		int next = (m+1)%12;
		int prev = (m+11)%12;

		// If a monthly mean value is outside of the allowed limits for daily
		// values (for instance negative radiation), we'll fail to make sure
		// the user knows the forcing data is broken.
		if (mvals[m] < minimum || mvals[m] > maximum) {
			fail("interp_monthly_means_conserve: Invalid monthly value given (%g), min = %g, max = %g",
				  mvals[m], minimum, maximum);
		}

		interp_single_month(mvals[prev], mvals[m], mvals[next],
		                    date.ndaymonth[m], dvals+start_of_month,
		                    minimum, maximum);

		start_of_month += date.ndaymonth[m];
	}

}


/// Climate interpolation from monthly totals to quasi-daily values
/** May be called from input/output module to generate daily climate values when
 *  raw data are on monthly basis.
 *
 *  The generated daily values will have the same monthly totals as the input.
 *
 *  \param mvals The monthly totals
 *  \param dvals The generated daily values
 */
void interp_monthly_totals_conserve(const double* mvals, double* dvals,
                                    double minimum, double maximum) {
	// Local date object just used to get number of days for each month
	Date date;

	// Convert monthly totals to mean daily values
	double mvals_daily[12];
	for (int m=0; m<12; m++)
		mvals_daily[m] = mvals[m] / (double)date.ndaymonth[m];

	interp_monthly_means_conserve(mvals_daily, dvals, minimum, maximum);
}

/// Distributes a single month of N deposition values
/** The dry component is simply spread out over all days, the
 *  wet deposition is distributed over days with precipitation
 *  (or evenly over all days if there is no precipitation).
 *
 *  \see distribute_ndep
 *
 *  \param NH4dry      Dry NH4 deposition (monthly mean of daily deposition)
 *  \param NO3dry      Dry NO3 deposition (monthly mean of daily deposition)
 *  \param NH4wet      Wet NH4 deposition (monthly mean of daily deposition)
 *  \param NO3wet      Wet NO3 deposition (monthly mean of daily deposition)
 *  \param time_steps  Number of days in the month
 *  \param dprec       Array of precipitation values
 *  \param dNH4dep     Output, total NH4 deposition for each day
 *  \param dNO3dep     Output, total NO3 deposition for each day
 */
void distribute_ndep_single_month(double NH4dry,double NO3dry, 
                                  double NH4wet,double NO3wet,
                                  int time_steps,
                                  const double* dprec,
                                  double* dNH4dep,double* dNO3dep) {

	// First count number of days with precipitation
	int raindays = 0;

	for (int i = 0; i < time_steps; i++) {
		if (!negligible(dprec[i])) {
			raindays++;
		}
	}

	// Distribute the values
	for (int i = 0; i < time_steps; i++) {

		// ndry is included in all days
		dNH4dep[i] = NH4dry;
		dNO3dep[i] = NO3dry;

		if (raindays == 0) {
			dNH4dep[i] += NH4wet;
			dNO3dep[i] += NO3wet;
		}
		else if (!negligible(dprec[i])) {
			dNH4dep[i] += (NH4wet*time_steps)/raindays;
			dNO3dep[i] += (NO3wet*time_steps)/raindays;
		}
	}
}

/// Distributes monthly mean N deposition values to daily values
/** \see distribute_ndep_single_month for details about how the
 *  distribution is done.
 *
 *  \param mndry Monthly means of daily dry N deposition
 *  \param mnwet Monthly means of daily wet N deposition
 *  \param dprec Daily precipitation data
 *  \param dNH4dep Output, total NH4 deposition for each day
 *  \param dNO3dep Output, total NO3 deposition for each day
 */
void distribute_ndep(const double* mNH4dry,const double* mNO3dry,
                     const double* mNH4wet,const double* mNO3wet,
					 const double* dprec, 
					 double* dNH4dep,double* dNO3dep) {

	Date date;
	int start_of_month = 0;

	for (int m = 0; m < 12; m++) {
		distribute_ndep_single_month(mNH4dry[m],mNO3dry[m],
			                         mNH4wet[m],mNO3wet[m],
									 date.ndaymonth[m],
		 	 	 	 	 	 	 	 dprec+start_of_month,dNH4dep+start_of_month,dNO3dep+start_of_month);

		start_of_month += date.ndaymonth[m];
	}
}

/// Distribution of monthly precipitation totals to quasi-daily values
/** \param mval_prec  total rainfall (mm) for month
 *  \param dval_prec  actual rainfall (mm) for each day of year
 *  \param mval_wet   expected number of rain days for month
 *  \param seed       seed for generating random numbers (\see randfrac)
 *  \param truncate   if set to true the function will set small daily values
 *                    (< 0.1) to zero
 */
void prdaily(double* mval_prec, double* dval_prec, double* mval_wet, long& seed, bool truncate /* = true */) {

//  Distribution of monthly precipitation totals to quasi-daily values
//  (From Dieter Gerten 021121)

	const double c1 = 1.0; // normalising coefficient for exponential distribution
	const double c2 = 1.2; // power for exponential distribution

	int m, d, dy, dyy, dy_hold;
	int daysum;
	double prob_rain; // daily probability of rain for this month
	double mprec; // average rainfall per rain day for this month
	double mprec_sum; // cumulative sum of rainfall for this month
		// (= mprecip in Dieter's code)
	double prob;

	dy = 0;
	daysum = 0;

	for (m=0; m<12; m++) {

		if (mval_prec[m] < 0.1) {

			// Special case if no rainfall expected for month

			for (d=0; d<date.ndaymonth[m]; d++) {
				dval_prec[dy] = 0.0;
				dy++;
			}
		}
		else {

			mprec_sum = 0.0;

			mval_wet[m] = max (mval_wet[m], 1.0);
				// force at least one rain day per month

			// rain on wet days (should be at least 0.1)
			mprec = max(mval_prec[m]/mval_wet[m], 0.1);
			mval_wet[m] = mval_prec[m] / mprec;

			prob_rain = mval_wet[m] / (double)date.ndaymonth[m];

			dy_hold = dy;

			while (negligible(mprec_sum)) {

				dy = dy_hold;

				for (d=0; d<date.ndaymonth[m]; d++) {

					// Transitional probabilities (Geng et al 1986)

					if (dy == 0) { // first day of year only
						prob = 0.75 * prob_rain;
					}
					else {
						if (dval_prec[dy-1] < 0.1)
							prob = 0.75 * prob_rain;
						else
							prob = 0.25 + (0.75 * prob_rain);
					}

					// Determine wet days randomly and use Krysanova/Cramer estimates of
					// parameter values (c1,c2) for an exponential distribution

					if (randfrac(seed)>prob)
						dval_prec[dy] = 0.0;
					else {
						double x=randfrac(seed);
						dval_prec[dy] = pow(-log(x), c2) * mprec * c1;
						if (dval_prec[dy] < 0.1) dval_prec[dy] = 0.0;
					}

					mprec_sum += dval_prec[dy];
					dy++;
				}

				// Normalise generated precipitation by prescribed monthly totals

				if (!negligible(mprec_sum)) {
					for (d=0; d<date.ndaymonth[m]; d++) {
						dyy = daysum + d;
						dval_prec[dyy] *= mval_prec[m] / mprec_sum;
						if (truncate && dval_prec[dyy] < 0.1) dval_prec[dyy] = 0.0;
					}
				}
			}
		}

		daysum += date.ndaymonth[m];
	}
}

/// Called each simulation day before any other driver or process functions
void dailyaccounting_gridcell(Gridcell& gridcell) {

	// DESCRIPTION
	// Updates daily climate parameters including growing degree day sums and
	// exponential temperature response term (gtemp, see below). Maintains monthly
	// and longer term records of variation in climate variables. PFT-specific
	// degree-day sums in excess of damaging temperatures are also calculated here.

	const double W11DIV12 = 11.0 / 12.0;
	const double W1DIV12 = 1.0 / 12.0;
	int y, startyear;

	// guess2008 - changed this from an int to a double
	double mtemp_last;

	Climate& climate = gridcell.climate;

	// On first day of the simulation ...
	if (iftwolayersoil && date.day == 0 && date.year == 0) {
		// Override some of the wetland Booleans when using the two layer soil. These have no effect in any case, as 
		// run_peatland must be false
		ifinundationstress = false;
		wetland_runon = 0.0;
		ifmethane = false;
		ifsaturatewetlands = false;

		// Fail in these cases
		if (iforganicsoilproperties || ifcarbonfreeze || ifmultilayersnow) {
			fail("dailyaccounting_gridcell: iforganicsoilproperties, ifcarbonfreeze and ifmultilayersnow must all be 0 if iftwolayersoil = 1");
		}
	}

	// On first day of year ...

	if (date.day == 0) {
		// ... reset annual GDD0 and GDD5 counter
		climate.agdd0 = 0.0;
		climate.agdd5 = 0.0;

		// reset annual nitrogen input variables
		gridcell.aNH4dep  = 0.0;
		gridcell.aNO3dep  = 0.0;
		climate.aprec = 0.0;

		// reset gridcell-level harvest fluxes
		gridcell.landcover.acflux_landuse_change=0.0;
		gridcell.landcover.acflux_harvest_slow=0.0;
		gridcell.landcover.anflux_landuse_change=0.0;
		gridcell.landcover.anflux_harvest_slow=0.0;

		for(int i=0;i<NLANDCOVERTYPES;i++) {
			gridcell.landcover.acflux_landuse_change_lc[i]=0.0;
			gridcell.landcover.acflux_harvest_slow_lc[i]=0.0;
			gridcell.landcover.anflux_landuse_change_lc[i]=0.0;
			gridcell.landcover.anflux_harvest_slow_lc[i]=0.0;
		}

		if (date.year == 0) {
			// First day of simulation - initialise running annual mean temperature and daily temperatures for the last month
			for (unsigned int d = 0; d < climate.dtemp_31.CAPACITY; d++) {
				climate.dtemp_31.add(climate.temp);
			}

			climate.atemp_mean = climate.temp;

			// Initialise gridcellpfts Michaelis-Menten kinetic Km value
			pftlist.firstobj();
			while (pftlist.isobj) {
				gridcell.pft[pftlist.getobj().id].Km = pftlist.getobj().km_volume * gridcell.soiltype.wtot;
				pftlist.nextobj();
			}
		}

		// Reset fluxes for all patches

		// Belongs perhaps in dailyaccounting_patch, but needs to be done before
		// landcover_dynamics because harvest flux is generated there.
		// N-flux variables moved here for easier balance accounting
		Gridcell::iterator gc_itr = gridcell.begin();
		while (gc_itr != gridcell.end()) {
			Stand& stand = *gc_itr;

			stand.firstobj();
			while (stand.isobj) {
				Patch& patch = stand.getobj();

				patch.fluxes.reset();
				patch.soil.anfix = 0.0;
				patch.soil.aorgNleach = 0.0;
				patch.soil.aorgCleach = 0.0;
				patch.soil.aminleach = 0.0;
				patch.anfert = 0.0;
				patch.managed_this_year = false;
				patch.plant_this_year = false;
				stand.nextobj();
			}

			++gc_itr;
		}
	}
	
	if ( (climate.lat >= 0.0 && date.day == COLDEST_DAY_NHEMISPHERE) ||
	     (climate.lat < 0.0 && date.day == COLDEST_DAY_SHEMISPHERE) ) {
		// In midwinter, reset GDD counter for summergreen phenology
		climate.gdd5 = 0.0;
		climate.ifsensechill = false;
	}
	else if ( (climate.lat >= 0.0 && date.day == WARMEST_DAY_NHEMISPHERE) ||
	          (climate.lat < 0.0 && date.day == WARMEST_DAY_SHEMISPHERE) ) {
		climate.ifsensechill = true;
	}

	// Update GDD counters and chill day count
	climate.gdd5 += max(0.0, climate.temp - 5.0);
	climate.agdd5 += max(0.0, climate.temp - 5.0);
	if (climate.temp < 5.0 && climate.chilldays <= Date::MAX_YEAR_LENGTH)
		climate.chilldays++;

	climate.gdd0 += max(0.0, climate.temp);
	climate.agdd0 += max(0.0, climate.temp);

	// Calculate gtemp (daily/sub-daily depending on the mode)
	if (date.diurnal()) {
		climate.gtemps.assign(date.subdaily, 0);
		for (int i=0; i<date.subdaily; i++) {
			respiration_temperature_response(climate.temps[i], climate.gtemps[i]);
		}
	}
	else {
		respiration_temperature_response(climate.temp, climate.gtemp);
	}

	// Sum annual nitrogen addition to system
	gridcell.aNH4dep += gridcell.dNH4dep;
	gridcell.aNO3dep += gridcell.dNO3dep;

	// Save yesterday's mean temperature for the last month
	mtemp_last = climate.mtemp;

	// Update daily temperatures, and mean overall temperature, for last 31 days
	climate.dtemp_31.add(climate.temp);
	climate.mtemp = climate.dtemp_31.mean();

	climate.dprec_31.add(climate.prec);
	climate.deet_31.add(climate.eet);

	// Reset GDD and chill day counter if mean monthly temperature falls below base
	// temperature
	if (mtemp_last >= 5.0 && climate.mtemp < 5.0 && climate.ifsensechill) {
		climate.gdd5 = 0.0;
		climate.chilldays = 0;
	}

	// Update fire related values
	if (firemodel == BLAZE) {
		simfire_accounting_gridcell(gridcell);
		blaze_accounting_gridcell(gridcell.climate);
	}

	// On last day of month ...

	if (date.islastday) {
		// Update mean temperature for the last 12 months
		// atemp_mean_new = atemp_mean_old * (11/12) + mtemp * (1/12)
		climate.atemp_mean = climate.atemp_mean * W11DIV12 + climate.mtemp * W1DIV12;

		// Record minimum and maximum monthly temperatures
		if (date.month == 0) {
			climate.mtemp_min = climate.mtemp;
			climate.mtemp_max = climate.mtemp;
		}
		else {
			if (climate.mtemp < climate.mtemp_min)
				climate.mtemp_min = climate.mtemp;
			if (climate.mtemp > climate.mtemp_max)
				climate.mtemp_max = climate.mtemp;
		}

		// On 31 December update records of minimum monthly temperatures for the last
		// 20 years and find mean of minimum monthly temperatures for the last 20 years
		if (date.islastmonth) {
			startyear = 20 - (int)min(19, date.year);
			climate.mtemp_min20 = climate.mtemp_min;
			climate.mtemp_max20 = climate.mtemp_max;

			for (y=startyear; y<20; y++) {
				climate.mtemp_min_20[y-1] = climate.mtemp_min_20[y];
				climate.mtemp_min20 += climate.mtemp_min_20[y];
				climate.mtemp_max_20[y-1] = climate.mtemp_max_20[y];
				climate.mtemp_max20 += climate.mtemp_max_20[y];
			}

			climate.mtemp_min20 /= (double)(21 - startyear);
			climate.mtemp_max20 /= (double)(21 - startyear);
			climate.mtemp_min_20[19] = climate.mtemp_min;
			climate.mtemp_max_20[19] = climate.mtemp_max;
			climate.agdd0_20.add(climate.agdd0);
		}

		climate.hmtemp_20[date.month].add(climate.dtemp_31.periodicmean(date.ndaymonth[date.month]));
		climate.hmprec_20[date.month].add(climate.dprec_31.periodicsum(date.ndaymonth[date.month]));
		climate.hmeet_20[date.month].add(climate.deet_31.periodicsum(date.ndaymonth[date.month]));
	}
}

void dailyaccounting_stand(Stand& stand) {
}

/// Manages C and N fluxes from slow harvest pools
void dailyaccounting_patch_lc(Patch& patch) {

	if (date.day > 0 || !ifslowharvestpool) {
		return;
	}

	Landcover& lc = patch.stand.get_gridcell().landcover;
	double scale = patch.stand.get_gridcell_fraction() / (double)patch.stand.nobj;

	pftlist.firstobj();
	while(pftlist.isobj) {				// NB. also inactive pft's
		Pft& pft = pftlist.getobj();
		Patchpft& ppft = patch.pft[pft.id];

		lc.acflux_harvest_slow += ppft.harvested_products_slow * pft.turnover_harv_prod * scale;
		lc.acflux_harvest_slow_lc[patch.stand.landcover] += ppft.harvested_products_slow * pft.turnover_harv_prod * scale;
		ppft.harvested_products_slow = ppft.harvested_products_slow * (1 - pft.turnover_harv_prod);

		lc.anflux_harvest_slow += ppft.harvested_products_slow_nmass * pft.turnover_harv_prod * scale;
		lc.anflux_harvest_slow_lc[patch.stand.landcover] += ppft.harvested_products_slow_nmass * pft.turnover_harv_prod * scale;
		ppft.harvested_products_slow_nmass = ppft.harvested_products_slow_nmass * (1 - pft.turnover_harv_prod);

		pftlist.nextobj();
	}
}

void dailyaccounting_patch(Patch& patch) {
	// DESCRIPTION
	// Updates daily soil parameters including exponential temperature response terms
	// (gtemp, see below). Maintains monthly and longer term records of variation in
	// soil variables. Initialises flux sums at start of simulation year.

	// INPUT AND OUTPUT PARAMETER
	// soil   = patch soil

	Soil& soil = patch.soil;

	if (date.day == 0) {

		patch.aaet = 0.0;
		patch.aintercep = 0.0;
		patch.apet = 0.0;

		// Calculate total FPC
		patch.fpc_total = 0;
		Vegetation& vegetation = patch.vegetation;
		vegetation.firstobj();
		while (vegetation.isobj) {
			patch.fpc_total += vegetation.getobj().fpc;		// indiv.fpc
			vegetation.nextobj();
		}
		// Calculate rescaling factor to account for overlap between populations/
		// cohorts/individuals (i.e. total FPC > 1)
		patch.fpc_rescale = 1.0 / max(patch.fpc_total, 1.0);
	}

	if (date.dayofmonth == 0) {

		soil.mwcontupper = 0.0;
		soil.mwcontlower = 0.0;

		patch.maet[date.month] = 0.0;
		patch.mintercep[date.month] = 0.0;
		patch.mpet[date.month]=0.0;
	}

	if(run_landcover)
		dailyaccounting_patch_lc(patch);

	// Store daily soil water in both layers
	soil.dwcontupper[date.day] = soil.get_soil_water_upper();
	soil.dwcontlower[date.day] = soil.get_soil_water_lower();

	soil.mwcontupper += soil.dwcontupper[date.day];
	soil.mwcontlower += soil.dwcontlower[date.day];

	// On last day of month, calculate mean content of upper and lower soil layers

	if (date.islastday) {

		soil.mwcontupper = mean(soil.dwcontupper + date.day - date.ndaymonth[date.month] + 1,
			date.ndaymonth[date.month]);

		// guess2008 - record water in lower layer too, and then update mwcont
		soil.mwcontlower = mean(soil.dwcontlower + date.day - date.ndaymonth[date.month] + 1,
			date.ndaymonth[date.month]);

		soil.mwcont[date.month][0] = soil.mwcontupper;
		soil.mwcont[date.month][1] = soil.mwcontlower;
	}

	// Reset monthly soil temp., litter depth, snow depth and thaw averages on Jan 1
	if (date.day == 0) {

		// Save the December value for use in establishment
		soil.dec_snowdepth = soil.msnowdepth[11];

		for (int mth = 0; mth < 12; mth++) {
			soil.msnowdepth[mth] = 0.0;
			soil.mthaw[mth] = 0.0;
			for (int sl = 0; sl < SOILTEMPOUT; sl++)
				soil.T_soil_monthly[mth][sl] = 0.0;
		}
	}

	// Calculate analytic soil temperatures at 25cm depth and update dtemp_31 (trend in daily air temperatures for the last 31 days)
	// Analytic soil temperatures can be used to test the accuracy of the numerical scheme in Soil::soil_temp_multilayer below.
	// The soil temperature at 25cm depth calculated here is used in the rest of the model (e.g. in respiration) if iftwolayersoil is true (determined in 
	// Soil::get_soil_temp_25() below).
	soil.soil_temp_analytic(patch.get_climate(), 0.25);

	// Calculate the soil temperature in each 10cm soil layer, the padding layers, and the snowpack. This function is called 
	// even if iftwolayersoil is true, though the calculations are much simpler. 
	bool validTemp = soil.soil_temp_multilayer(patch.get_climate().temp);
	if (!validTemp) fail("Error in Soil::soil_temp_multilayer");

	// Determine the soil temperature at 25cm depth
	double soiltemp25 = soil.get_soil_temp_25();

	if (iftwolayersoil) {
		// Update monthly 25cm soil temperature - used for output only
		soil.T_soil_monthly[date.month][2] += soiltemp25 / (double)date.ndaymonth[date.month];
	}

	respiration_temperature_response(soiltemp25, soil.gtemp);

	// Possible updates to soil.gtemp used if soiltemp25 < 0degC
	// Needed for root respiration
	if (ifcarbonfreeze && soiltemp25 <= 0.0 && !iftwolayersoil) {

		double decomp_at_freezing_point = exp(308.56*(1.0 / 56.02 - 1.0 / (0.0 + 46.02))); // soil.gtemp above when soiltemp25 = 0;

		// Linear approach (Koven et al. 2011)
		double slope = decomp_at_freezing_point / fabs(MIN_DECOMP_TEMP);

		if (soiltemp25 < MIN_DECOMP_TEMP)
			soil.gtemp = 0.0;
		else
			soil.gtemp = slope * soiltemp25 + decomp_at_freezing_point; // i.e. a linear decrease from decomp_at_freezing_point at 0C to 0 at MIN_DECOMP_TEMP
	}


	// On last day of month, calculate mean soil temperature at 25cm depth for last month

	soil.dtemp[date.dayofmonth] = soil.get_soil_temp_25();

	if (date.islastday)
		soil.mtemp = mean(soil.dtemp,date.ndaymonth[date.month]);


	double mdays = (double)date.ndaymonth[date.month];
	soil.msnowdepth[date.month] += soil.dsnowdepth / mdays;
	soil.mthaw[date.month] += soil.thaw / mdays;

	// needed for fire
	soil.dthaw[date.day] = soil.thaw;

	patch.is_litter_day = false;
	patch.isharvestday = false;
}


///////////////////////////////////////////////////////////////////////////////////////
// RESPIRATION TEMPERATURE RESPONSE
// Called by dailyaccounting_patch and dailyaccounting_gridcell to calculate
// response of respiration to temperature

void respiration_temperature_response(double temp,double& gtemp) {

	// DESCRIPTION
	// Calculates g(T), response of respiration rate to temperature (T), based on
	// empirical relationship for temperature response of soil temperature across
	// ecosystems, incorporating damping of Q10 response due to temperature
	// acclimation (Eqn 11, Lloyd & Taylor 1994)
	//
	//   r    = r10 * g(t)
	//   g(T) = EXP [308.56 * (1 / 56.02 - 1 / (T - 227.13))] (T in Kelvin)

	// INPUT PARAMETER
	// temp = air or soil temperature (deg C)

	// OUTPUT PARAMETER
	// gtemp = respiration temperature response

	if (temp >= -40.0) {
		gtemp = exp(308.56 * (1.0 / 56.02 - 1.0 / (temp + 46.02)));
	} 
	else {
		gtemp = 0.0;
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// DAYLENGTH, INSOLATION AND POTENTIAL EVAPOTRANSPIRATION
// Called by framework each simulation day following update of daily air temperature
// and before canopy exchange processes

void daylengthinsoleet(Climate& climate) {

	// Calculation of daylength, insolation and equilibrium evapotranspiration
	// for each day, given mean daily temperature, insolation (as percentage
	// of full sunshine or mean daily instantaneous downward shortwave
	// radiation flux, W/m2), latitude and day of year

	// INPUT AND OUTPUT PARAMETER
	// climate = gridcell climate

	const double QOO = 1360.0;
	const double BETA = 0.17;

	const double A = 107.0;
	const double B = 0.2;
	const double C = 0.25;
	const double D = 0.5;
	const double K = 13750.98708;
	const double FRADPAR = 0.5;
		// fraction of net incident shortwave radiation that is photosynthetically
		// active (PAR)

	double w, hn;

	//	CALCULATION OF NET DOWNWARD SHORT-WAVE RADIATION FLUX
	//	Refs: Prentice et al 1993, Monteith & Unsworth 1990,
	//	      Henderson-Sellers & Robinson 1986

	//	 (1) rs = (c + d*ni) * (1 - beta) * Qo * cos Z * k
	//	       (Eqn 7, Prentice et al 1993)
	//	 (2) Qo = Qoo * ( 1 + 2*0.01675 * cos ( 2*pi*(i+0.5)/365) )
	//	       (Eqn 8, Prentice et al 1993; angle in radians)
	//	 (3) cos Z = sin(lat) * sin(delta) + cos(lat) * cos(delta) * cos h
	//	       (Eqn 9, Prentice et al 1993)
	//	 (4) delta = -23.4 * pi / 180 * cos ( 2*pi*(i+10.5)/365 )
	//	       (Eqn 10, Prentice et al 1993, angle in radians)
	//	 (5) h = 2 * pi * t / 24 = pi * t / 12

	//	     where rs    = instantaneous net downward shortwave radiation
	//	                   flux, including correction for terrestrial shortwave albedo
	//	                   (W/m2 = J/m2/s)
	//	           c, d  = empirical constants (c+d = clear sky
	//	                   transmissivity)
	//	           ni    = proportion of bright sunshine
	//	           beta  = average 'global' value for shortwave albedo
	//	                   (not associated with any particular vegetation)
	//	           i     = julian day, (0-364, 0=1 Jan)
	//	           Qoo   = solar constant, 1360 W/m2
	//	           Z     = solar zenith angle (angular distance between the
	//	                   sun's rays and the local vertical)
	//	           k     = conversion factor from solar angular units to
	//	                   seconds, 12 / pi * 3600
	//	           lat   = latitude (+=N, -=S, in radians)
	//	           delta = solar declination (angle between the orbital
	//	                   plane and the Earth's equatorial plane) varying
	//	                   between +23.4 degrees in northern hemisphere
	//	                   midsummer and -23.4 degrees in N hemisphere
	//	                   midwinter
	//	           h     = hour angle, the fraction of 2*pi (radians) which
	//	                   the earth has turned since the local solar noon
	//	           t     = local time in hours from solar noon

	//	From (1) and (3), shortwave radiation flux at any hour during the
	//	day, any day of the year and any latitude given by
	//	 (6) rs = (c + d*ni) * (1 - beta) * Qo * ( sin(lat) * sin(delta) +
	//	          cos(lat) * cos(delta) * cos h ) * k
	//	Solar zenith angle equal to -pi/2 (radians) at sunrise and pi/2 at
	//	sunset.  For Z=pi/2 or Z=-pi/2,
	//	 (7) cos Z = 0
	//	From (3) and (7),
	//	 (8)  cos hh = - sin(lat) * sin(delta) / ( cos(lat) * cos(delta) )
	//	      where hh = half-day length in angular units
	//	Define
	//	 (9) u = sin(lat) * sin(delta)
	//	(10) v = cos(lat) * cos(delta)
	//	Thus
	//	(11) hh = acos (-u/v)
	//	To obtain the daily net downward short-wave radiation sum, integrate
	//	equation (6) from -hh to hh with respect to h,
	//	(12) rad = 2 * (c + d*ni) * (1 - beta) * Qo *
	//	              ( u*hh + v*sin(hh) )
	//	Define
	//	(13) w = (c + d*ni) * (1 - beta) * Qo
	//	From (12) & (13), and converting from angular units to seconds
	//	(14) rad = 2 * w * ( u*hh + v*sin(hh) ) * k

	if (!climate.doneday[date.day]) {

		// Calculate values of saved parameters for this day
		climate.qo[date.day] = QOO * (1.0 + 2.0 * 0.01675 *
							cos(2.0 * PI * ((double)date.day + 0.5) / date.year_length())); // Eqn 2
		double delta = -23.4 * DEGTORAD * cos(2.0 * PI * ((double)date.day + 10.5) / date.year_length());
				// Eqn 4, solar declination angle (radians)
		climate.u[date.day] = climate.sinelat * sin(delta); // Eqn 9
		climate.v[date.day] = climate.cosinelat * cos(delta); // Eqn 10

		if (climate.u[date.day] >= climate.v[date.day])
			climate.hh[date.day] = PI; // polar day
		else if (climate.u[date.day] <= -climate.v[date.day])
			climate.hh[date.day] = 0.0; // polar night
		else climate.hh[date.day] =
			acos(-climate.u[date.day] / climate.v[date.day]); // Eqn 11

		climate.sinehh[date.day] = sin(climate.hh[date.day]);

		// Calculate daylength in hours from hh

		climate.daylength_save[date.day] = 24.0 * climate.hh[date.day] / PI;
		climate.doneday[date.day] = true;
	}
	climate.daylength = climate.daylength_save[date.day];

	if (climate.instype == SUNSHINE) {		// insolation is percentage sunshine

		w = (C+D * climate.insol / 100.0) * (1.0 - BETA) * climate.qo[date.day]; // Eqn 13
		climate.rad = 2.0 * w * (climate.u[date.day] * climate.hh[date.day] +
				climate.v[date.day] * climate.sinehh[date.day]) * K; // Eqn 14

	}
	else { // insolation provided as instantaneous downward shortwave radiation flux

		// deal with the fact that insolation can be radiation during
		// daylight hours or during whole time step

		double averaging_period = 24 * 3600;

		if (climate.instype == NETSWRAD || climate.instype == SWRAD) {
			// insolation is provided as radiation during daylight hours
			averaging_period = climate.daylength_save[date.day] * 3600.0;
		}

		double net_coeff = 1;
		if (climate.instype == SWRAD || climate.instype == SWRAD_TS) {
			net_coeff = 1 - BETA; 			// albedo correction
		}
		climate.rad = climate.insol * net_coeff * averaging_period;

		// If using diurnal data with SWRAD or SWRAD_TS insolation type move
		// the following if-clause outside and below this if-else clause.
		if (date.diurnal()) {
			climate.pars.resize(date.subdaily);
			climate.rads.resize(date.subdaily);
			for (int i=0; i<date.subdaily; i++) {
				climate.rads[i] = climate.insols[i] * net_coeff * averaging_period;
				climate.pars[i] = climate.rads[i] * FRADPAR;
			}
		}

		// special case for polar night
		if (climate.hh[date.day] < 0.001) {
			w = 0;
		}
		else {
			w = climate.rad/2.0/(climate.u[date.day]*climate.hh[date.day]
				+climate.v[date.day]*climate.sinehh[date.day])/K; // from Eqn 14
		}
	}

	// Calculate PAR from radiation (Eqn A1, Haxeltine & Prentice 1996)
	climate.par = climate.rad * FRADPAR;

	//	CALCULATION OF DAILY EQUILIBRIUM EVAPOTRANSPIRATION
	//	(EET, or evaporative demand)
	//	Refs: Jarvis & McNaughton 1986, Prentice et al 1993

	//	(15) eet = ( s / (s + gamma) ) * rn / lambda
	//	       (Eqn 5, Prentice et al 1993)
	//	(16) s = 2.503E+6 * exp ( 17.269 * temp / (237.3 + temp) ) /
	//	         (237.3 + temp)**2
	//	       (Eqn 6, Prentice et al 1993)
	//	(17) rn = rs - rl
	//	(18) rl = ( b + (1-b) * ni ) * ( a - temp )
	//	       (Eqn 11, Prentice et al 1993)

	//	     where eet    = instantaneous evaporative demand (mm/s)
	//	           gamma  = psychrometer constant, c. 65 Pa/K
	//	           lambda = latent heat of vapourisation of water,
	//	                    c. 2.5E+6 J/kg
	//	           temp   = temperature (deg C)
	//	           rl     = net upward longwave radiation flux
	//	                    ('terrestrial radiation') (W/m2)
	//	           rn     = net downward radiation flux (W/m2)
	//	           a, b   = empirical constants

	//	Note: gamma and lambda are weakly dependent on temperature. Simple
	//	      linear functions are used to obtain approximate values for a
	//	      given temperature

	//	From (13) & (18),
	//	(19) rl = ( b + (1-b) * ( w / Qo / (1 - beta) - c ) / d ) * ( a - temp )

	//	Define
	//	(20) uu = w * u - rl
	//	(21) vv = w * v

	//	Daily EET sum is instantaneous EET integrated over the period
	//	during which rn >= 0.  Limits for the integration (half-period
	//	hn) are obtained by solving for

	//	(22) rn=0
	//	From (17) & (22),
	//	(23) rs - rl = 0
	//	From (6), (20), (21) and (23),
	//	(24) uu + vv * cos hn = 0
	//	From (24),
	//	(25) hn = acos ( -uu/vv )

	//	Integration of (15) w.r.t. h in the range -hn to hn leads to the
	//	following formula for total daily EET (mm)

	//	(26) eet_day = 2 * ( s / (s + gamma) / lambda ) *
	//	               ( uu*hn + vv*sin(hn) ) * k

	double rl = (B + (1.0 - B) * (w / climate.qo[date.day] / (1.0 - BETA) - C) / D) *
				(A - climate.temp); // Eqn 19: instantaneous net upward longwave radiation flux (W/m2)

	//	Calculate gamma and lambda
	double gamma = 65.05 + climate.temp * 0.064;
	double lambda = 2.495e6 - climate.temp * 2380.;

	double ct = 237.3 + climate.temp;
	double s = 2.503e6 * exp(17.269 * climate.temp / ct) / ct / ct;		// Eqn 16

	double uu = w * climate.u[date.day] - rl;			// Eqn 20
	double vv = w * climate.v[date.day];				// Eqn 21

	// Calculate half-period with positive net radiation, hn
	// In Eqn (25), hn defined for uu in range -vv to vv
	// For uu >= vv, hn = pi (12 hours, i.e. polar day)
	// For uu <= -vv, hn = 0 (i.e. polar night)
	if (uu>=vv) hn = PI; // polar day
	else if (uu<=-vv) hn = 0.0; // polar night
	else hn=acos(-uu / vv); // Eqn 25
	// Calculate total EET (equilibrium evapotranspiration) for this day, mm/day
	climate.eet = 2.0 * (s / (s + gamma) / lambda) * (uu * hn + vv * sin(hn)) * K;	// Eqn 26;
}

///////////////////////////////////////////////////////////////////////////////////////
// REFERENCES
//
// LPJF refers to the original FORTRAN implementation of LPJ as described by Sitch
//   et al 2000
// Carslaw, HS & Jaeger JC 1959 Conduction of Heat in Solids, Oxford University
//   Press, London
// Haxeltine A & Prentice IC 1996 BIOME3: an equilibrium terrestrial biosphere
//   model based on ecophysiological constraints, resource availability, and
//   competition among plant functional types. Global Biogeochemical Cycles 10:
//   693-709
// Henderson-Sellers, A & Robinson, PJ 1986 Contemporary Climatology. Longman,
//   Essex.
// Jarvis, PG & McNaughton KG 1986 Stomatal control of transpiration: scaling up
//   from leaf to region. Advances in Ecological Research 15: 1-49
// Jury WA, Gardner WR & Gardner WH 1991 Soil Physics 5th ed, John Wiley, NY
// Lloyd, J & Taylor JA 1994 On the temperature dependence of soil respiration
//   Functional Ecology 8: 315-323
// Parton, W. J., Hanson, P. J., Swanston, C., Torn, M., Trumbore, S. E., Riley, W.
//   & Kelly, R. 2010. ForCent model development and testing using the Enriched
//   Background Isotope Study experiment. Journal of Geophysical
//   Research-Biogeosciences, 115.
// Prentice, IC, Sykes, MT & Cramer W 1993 A simulation model for the transient
//   effects of climate change on forest landscapes. Ecological Modelling 65:
//   51-70.
// Press, WH, Teukolsky, SA, Vetterling, WT & Flannery, BT. (1986) Numerical
//   Recipes in FORTRAN, 2nd ed. Cambridge University Press, Cambridge
// Sitch, S, Prentice IC, Smith, B & Other LPJ Consortium Members (2000) LPJ - a
//   coupled model of vegetation dynamics and the terrestrial carbon cycle. In:
//   Sitch, S. The Role of Vegetation Dynamics in the Control of Atmospheric CO2
//   Content, PhD Thesis, Lund University, Lund, Sweden.
// Monteith, JL & Unsworth, MH 1990 Principles of Environmental Physics, 2nd ed,
//   Arnold, London
// van Duin, RHA 1963 The influence of soil management on the temperature
//   wave near the surface. Tech Bull 29 Inst for Land and Water Management
//   Research, Wageningen, Netherlands
