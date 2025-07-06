/*******************************************************************************
*   Copyright (C) 2024-2025 Cardinal Space Mining Club                         *
*                                                                              *
*                                 ;xxxxxxx:                                    *
*                                ;$$$$$$$$$       ...::..                      *
*                                $$$$$$$$$$x   .:::::::::::..                  *
*                             x$$$$$$$$$$$$$$::::::::::::::::.                 *
*                         :$$$$$&X;      .xX:::::::::::::.::...                *
*                 .$$Xx++$$$$+  :::.     :;:   .::::::.  ....  :               *
*                :$$$$$$$$$  ;:      ;xXXXXXXXx  .::.  .::::. .:.              *
*               :$$$$$$$$: ;      ;xXXXXXXXXXXXXx: ..::::::  .::.              *
*              ;$$$$$$$$ ::   :;XXXXXXXXXXXXXXXXXX+ .::::.  .:::               *
*               X$$$$$X : +XXXXXXXXXXXXXXXXXXXXXXXX; .::  .::::.               *
*                .$$$$ :xXXXXXXXXXXXXXXXXXXXXXXXXXXX.   .:::::.                *
*                 X$$X XXXXXXXXXXXXXXXXXXXXXXXXXXXXx:  .::::.                  *
*                 $$$:.XXXXXXXXXXXXXXXXXXXXXXXXXXX  ;; ..:.                    *
*                 $$& :XXXXXXXXXXXXXXXXXXXXXXXX;  +XX; X$$;                    *
*                 $$$: XXXXXXXXXXXXXXXXXXXXXX; :XXXXX; X$$;                    *
*                 X$$X XXXXXXXXXXXXXXXXXXX; .+XXXXXXX; $$$                     *
*                 $$$$ ;XXXXXXXXXXXXXXX+  +XXXXXXXXx+ X$$$+                    *
*               x$$$$$X ;XXXXXXXXXXX+ :xXXXXXXXX+   .;$$$$$$                   *
*              +$$$$$$$$ ;XXXXXXx;;+XXXXXXXXX+    : +$$$$$$$$                  *
*               +$$$$$$$$: xXXXXXXXXXXXXXX+      ; X$$$$$$$$                   *
*                :$$$$$$$$$. +XXXXXXXXX;      ;: x$$$$$$$$$                    *
*                ;x$$$$XX$$$$+ .;+X+      :;: :$$$$$xX$$$X                     *
*               ;;;;;;;;;;X$$$$$$$+      :X$$$$$$&.                            *
*               ;;;;;;;:;;;;;x$$$$$$$$$$$$$$$$x.                               *
*               :;;;;;;;;;;;;.  :$$$$$$$$$$X                                   *
*                .;;;;;;;;:;;    +$$$$$$$$$                                    *
*                  .;;;;;;.       X$$$$$$$:                                    *
*                                                                              *
*   Unless required by applicable law or agreed to in writing, software        *
*   distributed under the License is distributed on an "AS IS" BASIS,          *
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
*   See the License for the specific language governing permissions and        *
*   limitations under the License.                                             *
*                                                                              *
*******************************************************************************/

#pragma once

#include <stdio.h>

#include <array>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <string.h>

#include <sys/times.h>


namespace csm
{
namespace metrics
{

std::string cpuBrandString();
size_t numProcessors();
double cpuFreq(size_t p_num = 0);

#ifdef HAS_SENSORS
double readCpuTemp();
#endif

class ProcessStats
{
public:
    static void getMemAndThreads(double& resident_set_mb, size_t& num_threads);

public:
    ProcessStats();

public:
    void update();

    inline double currCpuPercent() const { return this->last_cpu_percent; }
    inline double avgCpuPercent() const { return this->avg_cpu_percent; }
    inline double maxCpuPercent() const { return this->max_cpu_percent; }

protected:
    clock_t last_cpu, last_sys_cpu, last_user_cpu;
    size_t cpu_samples{0}, num_processors{0};

    double last_cpu_percent{0.};
    double avg_cpu_percent{0.};
    double max_cpu_percent{0.};
};

template<typename Clock_T = std::chrono::system_clock>
class TaskStats_
{
public:
    using ClockT = Clock_T;
    using TimePoint = typename ClockT::time_point;

public:
    double addSample(const TimePoint& start, const TimePoint& end);

    inline double prevTime() const { return this->prev_time; }
    inline double avgTime() const { return this->avg_time; }
    inline double maxTime() const { return this->max_time; }
    inline double avgPeriod() const { return this->avg_period; }
    inline size_t numSamples() const { return this->samples; }

protected:
    TimePoint prev_sample_tp;

    double prev_time{0.};
    double avg_time{0.};
    double max_time{0.};
    double avg_period{0.};
    size_t samples{0};
};
using TaskStats = TaskStats_<>;





template<typename Clock_T>
double TaskStats_<Clock_T>::addSample(
    const TimePoint& start,
    const TimePoint& end)
{
    const double period =
        std::chrono::duration<double>(start - this->prev_sample_tp).count();
    const double dt = std::chrono::duration<double>(end - start).count();

    this->prev_sample_tp = start;
    this->prev_time = dt;

    const double w =
        (static_cast<double>(this->samples) /
         static_cast<double>(this->samples + 1));

    this->avg_time = (this->avg_time * w) + (dt * (1. - w));
    if (this->samples != 0)
    {
        this->avg_period = (this->avg_period * w) + (period * (1. - w));
    }
    this->samples++;

    if (dt > this->max_time)
    {
        this->max_time = dt;
    }

    return dt;
}

};  // namespace metrics
};  // namespace csm
