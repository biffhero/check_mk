#include "SectionManager.h"
#include "sections/SectionCheckMK.h"
#include "sections/SectionCrashDebug.h"
#include "sections/SectionDF.h"
#include "sections/SectionEventlog.h"
#include "sections/SectionFileinfo.h"
#include "sections/SectionGroup.h"
#include "sections/SectionLogwatch.h"
#include "sections/SectionMRPE.h"
#include "sections/SectionMem.h"
#include "sections/SectionOHM.h"
#include "sections/SectionPS.h"
#include "sections/SectionPluginGroup.h"
#include "sections/SectionServices.h"
#include "sections/SectionSkype.h"
#include "sections/SectionSpool.h"
#include "sections/SectionSystemtime.h"
#include "sections/SectionUptime.h"
#include "sections/SectionWMI.h"
#include "sections/SectionWinperf.h"

SectionManager::SectionManager(Configuration &config, const Environment &env)
    : _ps_use_wmi(config, "ps", "use_wmi", false)
    , _enabled_sections(config, "global", "sections")
    , _realtime_sections(config, "global", "realtime_sections")
    , _script_local_includes(config, "local", "include")
    , _script_plugin_includes(config, "plugin", "include")
    , _winperf_counters(config, "winperf", "counters") {
    loadStaticSections(config, env);
}

void SectionManager::emitConfigLoaded(const Environment &env) {
    for (const auto &section : _sections) {
        section->postprocessConfig(env);
    }
}

void SectionManager::addSection(Section *section) {
    _sections.push_back(std::unique_ptr<Section>(section));
}

bool SectionManager::sectionEnabled(const std::string &name) const {
    // if no sections were set, assume they are all enabled
    return !_enabled_sections.wasAssigned() ||
           (_enabled_sections->find(name) != _enabled_sections->end());
}

bool SectionManager::realtimeSectionEnabled(const std::string &name) const {
    return _realtime_sections->find(name) != _realtime_sections->end();
}

bool SectionManager::useRealtimeMonitoring() const {
    return _realtime_sections->size();
}

void SectionManager::loadStaticSections(Configuration &config,
                                        const Environment &env) {
    addSection(new SectionCrashDebug(config));
    addSection(new SectionCheckMK(config, env));
    addSection(new SectionUptime());
    addSection((new SectionDF())->withRealtimeSupport());
    addSection(new SectionPS(config));
    addSection((new SectionMem())->withRealtimeSupport());
    addSection(new SectionFileinfo(config));
    addSection(new SectionServices());

    addSection((new SectionWinperf("if"))->withBase(510));
    addSection((new SectionWinperf("phydisk"))->withBase(234));
    addSection((new SectionWinperf("processor"))
                   ->withBase(238)
                   ->withRealtimeSupport());

    for (winperf_counter *counter : *_winperf_counters) {
        if (counter->id != -1) {
            addSection((new SectionWinperf(counter->name.c_str()))
                           ->withBase(counter->id));
        }
    }

    addSection(new SectionEventlog(config));
    addSection(new SectionLogwatch(config, env));

    addSection((new SectionWMI("dotnet_clrmemory"))
                   ->withObject(L"Win32_PerfRawData_NETFramework_NETCLRMemory")
                   ->withToggleIfMissing());

    addSection((new SectionGroup("wmi_cpuload"))
                   ->withToggleIfMissing()
                   ->withNestedSubtables()
                   ->withSubSection(
                       (new SectionWMI("system_perf"))
                           ->withObject(L"Win32_PerfRawData_PerfOS_System"))
                   ->withSubSection((new SectionWMI("computer_system"))
                                        ->withObject(L"Win32_ComputerSystem"))
                   ->withSeparator(','));

    addSection(
        (new SectionGroup("msexch"))
            ->withToggleIfMissing()
            ->withSubSection((new SectionWMI("msexch_activesync"))
                                 ->withObject(L"MSExchangeActiveSync"))
            ->withSubSection((new SectionWMI("msexch_availability"))
                                 ->withObject(L"MSExchangeAvailabilityService"))
            ->withSubSection(
                (new SectionWMI("msexch_owa"))->withObject(L"MSExchangeOWA"))
            ->withSubSection((new SectionWMI("msexch_autodiscovery"))
                                 ->withObject(L"MSExchangeAutodiscover"))
            ->withSubSection((new SectionWMI("msexch_isclienttype"))
                                 ->withObject(L"MSExchangeISClientType"))
            ->withSubSection((new SectionWMI("msexch_isstore"))
                                 ->withObject(L"MSExchangeISStore"))
            ->withSubSection((new SectionWMI("msexch_rpcclientaccess"))
                                 ->withObject(L"MSExchangeRpcClientAccess"))
            ->withHiddenHeader()
            ->withSeparator(','));

    addSection(new SectionSkype());

    addSection((new SectionWMI("wmi_webservices"))
                   ->withObject(L"Win32_PerfRawData_W3SVC_WebService")
                   ->withToggleIfMissing());

    addSection((new SectionOHM(config, env))
                   ->withColumns({L"Index", L"Name", L"Parent", L"SensorType",
                                  L"Value"}));

    addSection(new SectionPluginGroup(config, env.localDirectory(), LOCAL));
    for (const auto &include : *_script_local_includes) {
        addSection(new SectionPluginGroup(config, include.second, LOCAL,
                                          include.first));
    }

    addSection(new SectionPluginGroup(config, env.pluginsDirectory(), PLUGIN));
    for (const auto &include : *_script_plugin_includes) {
        addSection(new SectionPluginGroup(config, include.second, PLUGIN,
                                          include.first));
    }

    addSection(new SectionSpool());
    addSection(new SectionMRPE(config));

    addSection(new SectionSystemtime());
}
