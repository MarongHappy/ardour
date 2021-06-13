/*
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "gtkmm2ext/utils.h"

#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/audio_backend.h"

#include "dsp_stats_ui.h"
#include "timers.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace Gtk;

DspStatisticsGUI::DspStatisticsGUI ()
	: buffer_size_label ("", ALIGN_RIGHT, ALIGN_CENTER)
	, reset_button (_("Reset"))
{
	const size_t nlabels = Session::NTT + AudioEngine::NTT + AudioBackend::NTT;

	labels = new Label*[nlabels];
	for (size_t n = 0; n < nlabels; ++n) {
		labels[n] = new Label ("", ALIGN_RIGHT, ALIGN_CENTER);
		set_size_request_to_display_given_text (*labels[n], string_compose (_("%1 (%2 - %3 .. %4 "), 10000, 1000, 10000, 1000), 0, 0);
	}

	int row = 0;

	attach (*manage (new Gtk::Label (_("Buffer size: "), ALIGN_RIGHT, ALIGN_CENTER)), 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (buffer_size_label, 1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	attach (*manage (new Gtk::Label (_("Idle: "), ALIGN_RIGHT, ALIGN_CENTER)), 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (*labels[AudioEngine::NTT + Session::NTT + AudioBackend::DeviceWait], 1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	attach (*manage (new Gtk::Label (_("DSP: "), ALIGN_RIGHT, ALIGN_CENTER)), 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (*labels[AudioEngine::NTT + Session::NTT + AudioBackend::RunLoop], 1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	attach (*manage (new Gtk::Label (_("Engine: "), ALIGN_RIGHT, ALIGN_CENTER)), 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (*labels[AudioEngine::ProcessCallback], 1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	attach (*manage (new Gtk::Label (_("Session: "), ALIGN_RIGHT, ALIGN_CENTER)), 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (*labels[AudioEngine::NTT + Session::OverallProcess], 1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	attach (reset_button, 1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);

	reset_button.signal_clicked().connect (sigc::mem_fun (*this, &DspStatisticsGUI::reset_button_clicked));

	show_all ();
}

void
DspStatisticsGUI::reset_button_clicked ()
{
	if (_session) {
		for (size_t n = 0; n < Session::NTT; ++n) {
			_session->dsp_stats[n].queue_reset ();
		}
	}
	for (size_t n = 0; n < AudioEngine::NTT; ++n) {
		AudioEngine::instance()->dsp_stats[n].queue_reset ();
	}
	for (size_t n = 0; n < AudioBackend::NTT; ++n) {
		AudioEngine::instance()->current_backend()->dsp_stats[n].queue_reset ();
	}
}

void
DspStatisticsGUI::start_updating ()
{
	update ();
	update_connection = Timers::second_connect (sigc::mem_fun(*this, &DspStatisticsGUI::update));
}

void
DspStatisticsGUI::stop_updating ()
{
	update_connection.disconnect ();
}

void
DspStatisticsGUI::update ()
{
	uint64_t min = 0;
	uint64_t max = 0;
	double   avg = 0.;
	double   dev = 0.;
	char buf[64];

	int bufsize = AudioEngine::instance()->samples_per_cycle ();
	double bufsize_usecs = (bufsize * 1000000.0) / AudioEngine::instance()->sample_rate();
	double bufsize_msecs = (bufsize * 1000.0) / AudioEngine::instance()->sample_rate();
	snprintf (buf, sizeof (buf), "%d samples / %5.2f msecs", bufsize, bufsize_msecs);
	buffer_size_label.set_text (buf);

	if (AudioEngine::instance()->current_backend()->dsp_stats[AudioBackend::DeviceWait].get_stats (min, max, avg, dev)) {

		if (avg > 1000.0) {
			avg /= 1000.0;
			dev /= 1000.0;
			min = (uint64_t) floor (min / 1000.0);
			max = (uint64_t) floor (max / 1000.0);
			snprintf (buf, sizeof (buf), "%7.2f msec %5.2f%% (%" PRId64 " - %" PRId64 " .. %7.2g)", avg, (100.0 * avg) / bufsize_msecs, min, max, dev);
		} else {
			snprintf (buf, sizeof (buf), "%7.2f usec %5.2f%% (%" PRId64 " - %" PRId64 " .. %7.2g)", avg, (100.0 * avg) / bufsize_usecs, min, max, dev);
		}
		labels[AudioEngine::NTT + Session::NTT + AudioBackend::DeviceWait]->set_text (buf);
	} else {
		labels[AudioEngine::NTT + Session::NTT + AudioBackend::DeviceWait]->set_text (_("not measured"));
	}

	if (AudioEngine::instance()->current_backend()->dsp_stats[AudioBackend::RunLoop].get_stats (min, max, avg, dev)) {

		if (max > 1000.0) {
			double maxf = max / 1000.0;
			snprintf (buf, sizeof (buf), "%7.2f msec %5.2f%%", maxf, (100.0 * maxf) / bufsize_msecs);
		} else {
			snprintf (buf, sizeof (buf), "%" PRId64 " usec %5.2f%%", max, (100.0 * max) / bufsize_usecs);
		}
		labels[AudioEngine::NTT + Session::NTT + AudioBackend::RunLoop]->set_text (buf);
	} else {
		labels[AudioEngine::NTT + Session::NTT + AudioBackend::RunLoop]->set_text (_("not measured"));
	}

	AudioEngine::instance()->dsp_stats[AudioEngine::ProcessCallback].get_stats (min, max, avg, dev);

	if (_session) {

		uint64_t smin = 0;
		uint64_t smax = 0;
		double   savg = 0.;
		double   sdev = 0.;

		_session->dsp_stats[AudioEngine::ProcessCallback].get_stats (smin, smax, savg, sdev);

		snprintf (buf, sizeof (buf), "%7.2f usec %5.2f%% (%" PRId64 " - %" PRId64 " .. %7.2g)", savg, (100.0 * savg) / bufsize_usecs, smin, smax, sdev);
		labels[AudioEngine::NTT + Session::OverallProcess]->set_text (buf);

		/* Subtract session time from engine process time to show
		 * engine overhead
		 */

		min -= smin;
		max -= smax;
		avg -= savg;
		dev -= sdev;

		snprintf (buf, sizeof (buf), "%7.2f usec %5.2f%% (%" PRId64 " - %" PRId64 " .. %7.2g)", avg, (100.0 * avg) / bufsize_usecs, min, max, dev);
		labels[AudioEngine::ProcessCallback]->set_text (buf);

	} else {

		snprintf (buf, sizeof (buf), "%7.2f usec %5.2f%% (%" PRId64 " - %" PRId64 " .. %7.2g)", avg, (100.0 * avg) / bufsize_usecs, min, max, dev);
		labels[AudioEngine::ProcessCallback]->set_text (buf);
		labels[AudioEngine::NTT + Session::OverallProcess]->set_text (_("No session loaded"));
	}
}
