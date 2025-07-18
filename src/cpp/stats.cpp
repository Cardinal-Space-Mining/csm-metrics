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

#include <csm_metrics/stats.hpp>

#include <sstream>
#include <fstream>

#ifdef HAS_SENSORS
    #include <sensors/sensors.h>
#endif

#include <unistd.h>
#ifdef HAS_CPUID
    #include <cpuid.h>
#endif

#include <sys/sysinfo.h>


namespace csm
{
namespace metrics
{

std::string cpuBrandString()
{
#ifdef HAS_CPUID
    std::array<char, 0x40> CPUBrandString{};
    std::array<unsigned int, 4> CPUInfo{};

    __cpuid(0x80000000, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);

    unsigned int nExIds = CPUInfo[0];
    for (unsigned int i = 0x80000000; i <= nExIds; ++i)
    {
        __cpuid(i, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
        if (i == 0x80000002)
        {
            memcpy(CPUBrandString.data(), CPUInfo.data(), sizeof(CPUInfo));
        }
        else if (i == 0x80000003)
        {
            memcpy(&CPUBrandString[16], CPUInfo.data(), sizeof(CPUInfo));
        }
        else if (i == 0x80000004)
        {
            memcpy(&CPUBrandString[32], CPUInfo.data(), sizeof(CPUInfo));
        }
    }

    return std::string{CPUBrandString.data()};
#else
    return "";
#endif
}

size_t numProcessors()
{
    return get_nprocs_conf();  // additionally there is std::thread::hardware_concurrency() if you want to remain portable
}

double cpuFreq(size_t p_num)
{
#if 0
    char file_path[250];
    if(std::snprintf(file_path, sizeof(file_path) - 1, "/sys/devices/system/cpu/cpufreq/policy%zu/scaling_cur_freq", p_num) < 0)
    {
        return 0;
    }

    FILE* file = fopen (file_path, "r");
    if(!file) return 0;

    int freq = 0;
    if(std::fscanf(file, "%d", &freq) == EOF) return 0;

    std::fclose(file);

    return freq * 1000.;
#else
    double value = 0.;
    try
    {
        std::ifstream file{(std::ostringstream{}
                            << "/sys/devices/system/cpu/cpufreq/policy" << p_num
                            << "/scaling_cur_freq")
                               .str()};
        file >> value;
        file.close();
    }
    catch (...)
    {
    }
    return value * 1000.;
#endif
}

#ifdef HAS_SENSORS
class ReadCPUTempContext
{
public:
    inline ReadCPUTempContext()
    {
        // Init sensors library
        have_sensors_initialized = sensors_init(NULL);


        if (have_sensors_initialized != 0)
        {
            return;
        }

        // Init Chip
        int chip_nr = 0;
        this->chip = sensors_get_detected_chips(NULL, &chip_nr);

        if (!this->chip)
        {
            return;
        }

        // Init Feature
        int feature_nr = 0;
        this->feature = sensors_get_features(chip, &feature_nr);

        if (!this->feature || feature->type != SENSORS_FEATURE_TEMP)
        {
            return;
        }

        subfeature = sensors_get_subfeature(
            chip,
            feature,
            SENSORS_SUBFEATURE_TEMP_INPUT);
    }

    inline double get_cpu_tmp()
    {
        double temp_value = -1;
        if (subfeature)
        {
            if (sensors_get_value(chip, subfeature->number, &temp_value) != 0)
            {
                temp_value = -1;
            }
        }
        return temp_value;
    }

    inline ~ReadCPUTempContext()
    {
        if (have_sensors_initialized == 0)
        {
            sensors_cleanup();
        }
    }

private:
    int have_sensors_initialized = 1;
    const sensors_chip_name* chip = nullptr;
    const sensors_feature* feature = nullptr;
    const sensors_subfeature* subfeature = nullptr;
};


double readCpuTemp()
{
    static ReadCPUTempContext ctx;
    return ctx.get_cpu_tmp();
}
#else
double readCpuTemp()
{
    return 0.;
}
#endif



void ProcessStats::getMemAndThreads(
    double& resident_set_mb,
    size_t& num_threads)
{
    resident_set_mb = 0.;
    num_threads = 0;
    std::string _buff;
    long rss = 0;

    try
    {
        std::ifstream f{"/proc/self/stat", std::ios_base::in};
        f >> _buff                   //pid
            >> _buff                 //comm
            >> _buff                 //state
            >> _buff                 //ppid
            >> _buff                 //pgrp
            >> _buff                 //session
            >> _buff                 //tty_nr
            >> _buff                 //tpgid
            >> _buff                 //flags
            >> _buff                 //minflt
            >> _buff                 //cminflt
            >> _buff                 //majflt
            >> _buff                 //cmajflt
            >> _buff                 //utime
            >> _buff                 //stime
            >> _buff                 //cutime
            >> _buff                 //cstime
            >> _buff                 //priority
            >> _buff                 //nice
            >> num_threads >> _buff  //itrealvalue
            >> _buff                 //starttime
            >> _buff                 //vsize
            >> rss;                  // don't care about the rest
        f.close();
    }
    catch (...)
    {
    }

    long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024;
    resident_set_mb = rss * page_size_kb / 1000.;
}





ProcessStats::ProcessStats() : num_processors{csm::metrics::numProcessors()}
{
    struct tms time_sample{};

    this->last_cpu = times(&time_sample);
    this->last_sys_cpu = time_sample.tms_stime;
    this->last_user_cpu = time_sample.tms_utime;
}

void ProcessStats::update()
{
    struct tms _sample{};
    clock_t now = times(&_sample);

    if (now > this->last_cpu && _sample.tms_stime >= this->last_sys_cpu &&
        _sample.tms_utime >= this->last_user_cpu)
    {
        this->last_cpu_percent =
            ((_sample.tms_stime - this->last_sys_cpu) +
             (_sample.tms_utime - this->last_user_cpu)) *
            (100. / (now - this->last_cpu) / this->num_processors);
    }
    this->last_cpu = now;
    this->last_sys_cpu = _sample.tms_stime;
    this->last_user_cpu = _sample.tms_utime;

    this->avg_cpu_percent =
        (this->avg_cpu_percent * this->cpu_samples + this->last_cpu_percent) /
        (this->cpu_samples + 1);
    this->cpu_samples++;

    if (this->last_cpu_percent > this->max_cpu_percent)
    {
        this->max_cpu_percent = this->last_cpu_percent;
    }
}

ProcessStatsMsg& ProcessStats::toMsg(ProcessStatsMsg& msg) const
{
    double mem;
    size_t n_threads;

    getMemAndThreads(mem, n_threads);

    msg.cpu_percent = static_cast<float>(this->last_cpu_percent);
    msg.cpu_temp = static_cast<float>(readCpuTemp());
    msg.mem_usage_mb = static_cast<float>(mem);
    msg.num_threads = static_cast<uint32_t>(n_threads);

    return msg;
}

};  // namespace metrics
};  // namespace csm
