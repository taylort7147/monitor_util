// monitor_util.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <highlevelmonitorconfigurationapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>
#include <WinUser.h>

#include <cctype>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <thread>
#include <variant>
#include <vector>


void PrintLastError()
{
  const auto errorCode = GetLastError();
  if (errorCode != 0)
  {
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER |
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      errorCode,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&messageBuffer,
      0,
      NULL);

    std::cout << "Error [" << errorCode << "] " << messageBuffer << std::endl;
  }
  else
  {
    std::cout << "No error" << std::endl;
  }
}

class MonitorUtils
{
public:

  struct HighLevelCapabilities
  {
    HighLevelCapabilities() = default;
    HighLevelCapabilities(DWORD cap)
    {
      Set(cap);
    }

    void Set(DWORD cap)
    {
      Valid = true;
      None = cap & MC_CAPS_NONE;
      Brightness = cap & MC_CAPS_BRIGHTNESS;
      ColorTemperature = cap & MC_CAPS_COLOR_TEMPERATURE;
      Contrast = cap & MC_CAPS_CONTRAST;
      Degauss = cap & MC_CAPS_DEGAUSS;
      DisplayAreaPosition = cap & MC_CAPS_DISPLAY_AREA_POSITION;
      DisplayAreaSize = cap & MC_CAPS_DISPLAY_AREA_SIZE;
      MonitorTechnologyType = cap & MC_CAPS_MONITOR_TECHNOLOGY_TYPE;
      RedGreenBlueDrive = cap & MC_CAPS_RED_GREEN_BLUE_DRIVE;
      RedGreenBlueGain = cap & MC_CAPS_RED_GREEN_BLUE_GAIN;
      RestoreFactoryColorDefaults = cap & MC_CAPS_RESTORE_FACTORY_COLOR_DEFAULTS;
      RestoreFactoryDefaults = cap & MC_CAPS_RESTORE_FACTORY_DEFAULTS;
      RestoreFactoryDefaultsEnablesMonitorSettings = cap & MC_RESTORE_FACTORY_DEFAULTS_ENABLES_MONITOR_SETTINGS;
    }

    bool Valid{ false };
    bool None{ false };
    bool Brightness{ false };
    bool ColorTemperature{ false };
    bool Contrast{ false };
    bool Degauss{ false };
    bool DisplayAreaPosition{ false };
    bool DisplayAreaSize{ false };
    bool MonitorTechnologyType{ false };
    bool RedGreenBlueDrive{ false };
    bool RedGreenBlueGain{ false };
    bool RestoreFactoryColorDefaults{ false };
    bool RestoreFactoryDefaults{ false };
    bool RestoreFactoryDefaultsEnablesMonitorSettings{ false };
  };

  enum class VCPCapabilityElementType
  {
    Tree,
    Leaf
  };

  enum class VCPCapabilityValueType
  {
    Text,
    VCPCode
  };

  struct VCPCapabilityElement
  {
    std::variant<std::string, int> Value;
    std::vector<VCPCapabilityElement> Children;
    VCPCapabilityElementType ElementType;
    VCPCapabilityValueType ValueType;
  };

  struct Token
  {
    std::string Value;
    std::vector<Token> Children;
  };

  class Monitor
  {
  public:
    ~Monitor()
    {
      FreePhysicalHandle();
    }

    void SetHandle(HMONITOR handle)
    {
      FreePhysicalHandle();
      m_handle = handle;
      if (m_handle)
      {
        DWORD numPhysicalMonitors = 0;
        if (GetNumberOfPhysicalMonitorsFromHMONITOR(m_handle, &numPhysicalMonitors) && numPhysicalMonitors == 1)
        {
          PHYSICAL_MONITOR physicalMonitor;
          if (GetPhysicalMonitorsFromHMONITOR(m_handle, 1, &physicalMonitor))
          {
            m_physicalHandle = physicalMonitor;
          }
        }
      }
    }

    HMONITOR GetHandle() const
    {
      return m_handle;
    }

    PHYSICAL_MONITOR GetPhysicalHandle() const
    {
      return m_physicalHandle;
    }

  private:

    void FreePhysicalHandle()
    {
      if (m_physicalHandle.hPhysicalMonitor)
      {
        DestroyPhysicalMonitors(1, &m_physicalHandle);
        m_physicalHandle.hPhysicalMonitor = {};
      }
    }

    HMONITOR m_handle{ nullptr };
    PHYSICAL_MONITOR m_physicalHandle{ nullptr };
  };

  union MonitorParam
  {
    HMONITOR Monitor;
    int Index;
  };
  static_assert(sizeof MonitorParam == sizeof DWORD, "Invalid size");


  static BOOL CALLBACK GetMonitorByIndex(
    HMONITOR monitor,
    HDC deviceContext,
    LPRECT rect,
    LPARAM applicationDefinedData
  )
  {
    auto monitorParam = reinterpret_cast<MonitorParam*>(applicationDefinedData);
    if (--monitorParam->Index < 0)
    {
      monitorParam->Monitor = monitor;
      return FALSE; // Stop enumeration
    }
    return TRUE; // Continue enumeration
  }

  static Monitor GetMonitor(int index)
  {
    Monitor monitor{};

    auto hdc = GetDC(NULL);
    MonitorParam monitorParam{};
    monitorParam.Index = index;
    (void)EnumDisplayMonitors(hdc, NULL, MonitorUtils::GetMonitorByIndex, reinterpret_cast<LPARAM>(&monitorParam));
    if (monitorParam.Index != -1)
    {
      monitor.SetHandle(monitorParam.Monitor);
    }
    return monitor;
  }

  static bool SetVCPFeature(Monitor monitor, uint8_t code, uint32_t value)
  {
    return ::SetVCPFeature(
      monitor.GetPhysicalHandle().hPhysicalMonitor,
      code,
      value);
  }

  struct VCPFeatureResult
  {
    bool Success;
    MC_VCP_CODE_TYPE CodeType;
    DWORD CurrentValue;
    DWORD MaxValue;
  };

  static VCPFeatureResult GetVCPFeature(Monitor monitor, uint8_t code)
  {
    VCPFeatureResult value{};
    value.Success = false;
    value.Success = ::GetVCPFeatureAndVCPFeatureReply(
      monitor.GetPhysicalHandle().hPhysicalMonitor,
      code,
      &value.CodeType,
      &value.CurrentValue,
      &value.MaxValue);
    return value;
  }

  struct MonitorInfo
  {
    std::string Name{ "Invalid" };
    bool Primary{ false };
  };

  static MonitorInfo GetMonitorInfo(Monitor monitor)
  {
    MonitorInfo monitorInfo{};
    MONITORINFOEX winMonitorInfo{};
    winMonitorInfo.cbSize = sizeof winMonitorInfo;
    if (::GetMonitorInfo(monitor.GetHandle(), &winMonitorInfo) != 0)
    {
      monitorInfo.Primary = winMonitorInfo.dwFlags & MONITORINFOF_PRIMARY;
      monitorInfo.Name = winMonitorInfo.szDevice;
    }
    return monitorInfo;
  }

  struct LowLevelCapabilities
  {
    bool Valid{ false };
    VCPCapabilityElement Capabilities; // Root capabilities element
  };

  static HighLevelCapabilities GetHighLevelCapabilities(Monitor monitor)
  {
    DWORD highLevelCapabilitiesRaw = 0;
    DWORD supportedColorTemperatures = 0;
    HighLevelCapabilities highLevelCapabilities{};
    if (GetMonitorCapabilities(monitor.GetPhysicalHandle().hPhysicalMonitor, &highLevelCapabilitiesRaw, &supportedColorTemperatures))
    {
      highLevelCapabilities.Set(highLevelCapabilitiesRaw);
    }

    return highLevelCapabilities;
  }

  static LowLevelCapabilities GetLowLevelCapabilities(Monitor monitor)
  {
    LowLevelCapabilities capabilities{};
    if (true/*(monitor.GetPhysicalHandle().hPhysicalMonitor*/)
    {
      DWORD lowLevelCapabilitiesStringLength = 0;
      if (GetCapabilitiesStringLength(
        monitor.GetPhysicalHandle().hPhysicalMonitor,
        &lowLevelCapabilitiesStringLength))
      {
        std::vector<char> buffer;
        buffer.resize(lowLevelCapabilitiesStringLength);
        if (CapabilitiesRequestAndCapabilitiesReply(
          monitor.GetPhysicalHandle().hPhysicalMonitor,
          buffer.data(),
          lowLevelCapabilitiesStringLength))
        {
          std::string lowLevelCapabilitiesString{ buffer.data() };

          std::string indent = "";
          const auto tokens = TokenizeLowLevelCapabilitiesString(lowLevelCapabilitiesString);
          const auto elements = ParseLowLevelCapabilitiesString(lowLevelCapabilitiesString);
          if (!elements.empty())
          {
            capabilities.Capabilities = elements.at(0);
            capabilities.Valid = true;
          }
        }
      }
    }

    return capabilities;
  }











  static int TokenizeLowLevelCapabilitiesString(const std::string& capabilities, std::vector<Token>* tokens)
  {
    Token token;
    int index = 0;
    std::vector<char> characters{ capabilities.begin(), capabilities.end() };
    for (index = 0; index < static_cast<int>(characters.size()); ++index)
    {
      const auto c = characters.at(index);
      if (c == '(')
      {
        const auto substring = capabilities.substr(index + 1);
        index += TokenizeLowLevelCapabilitiesString(substring, &token.Children) + 1;
        tokens->push_back(token);
        token.Value.clear();
        token.Children.clear();
      }
      else if (c == ')')
      {
        tokens->push_back(token);
        break;
      }
      else if (c == ' ')
      {
        tokens->push_back(token);
        token.Value.clear();
        token.Children.clear();
      }
      else
      {
        token.Value += c;
      }
    }
    return index;
  }

  static std::vector<Token> TokenizeLowLevelCapabilitiesString(const std::string& capabilities)
  {
    std::vector<Token> tokens;
    TokenizeLowLevelCapabilitiesString(capabilities, &tokens);
    return tokens;
  }

  static void Print(Token const& token, std::string const& indent = "")
  {
    std::cout << indent << token.Value;
    if (!token.Children.empty())
    {
      std::cout << "(" << std::endl;
      for (const auto& child : token.Children)
      {
        Print(child, indent + "  ");
      }
      std::cout << indent << ")" << std::endl;
    }
    std::cout << std::endl;
  }

  static void Print(HighLevelCapabilities const& capabilities, std::string const& indent = "")
  {
    if (capabilities.None)
    {
      std::cout << indent << "None" << std::endl;
    }
    else
    {
      std::cout << indent << "Brightness: " << capabilities.Brightness << std::endl;
      std::cout << indent << "Color temperature: " << capabilities.ColorTemperature << std::endl;
      std::cout << indent << "Contrast: " << capabilities.Contrast << std::endl;
      std::cout << indent << "Degauss: " << capabilities.Degauss << std::endl;
      std::cout << indent << "Display area position: " << capabilities.DisplayAreaPosition << std::endl;
      std::cout << indent << "Display area size: " << capabilities.DisplayAreaSize << std::endl;
      std::cout << indent << "Monitor technology type: " << capabilities.MonitorTechnologyType << std::endl;
      std::cout << indent << "RGB drive: " << capabilities.RedGreenBlueDrive << std::endl;
      std::cout << indent << "RGB gain: " << capabilities.RedGreenBlueGain << std::endl;
      std::cout << indent << "Restore factory color defaults: " << capabilities.RestoreFactoryColorDefaults << std::endl;
      std::cout << indent << "Restore factory defaults: " << capabilities.RestoreFactoryDefaults << std::endl;
      std::cout << indent << "Restore factory defaults enables monitor settings: " << capabilities.RestoreFactoryDefaultsEnablesMonitorSettings << std::endl;
    }
  }

  static void Print(VCPCapabilityElement const& element, std::string const& indent = "")
  {
    if (element.ValueType == VCPCapabilityValueType::VCPCode)
    {
      std::cout << indent << std::hex << "0x" << std::get<int>(element.Value);
    }
    else
    {
      std::cout << indent << std::get<std::string>(element.Value);
    }
    if (!element.Children.empty())
    {
      std::cout << "(" << std::endl;
      for (const auto& child : element.Children)
      {
        Print(child, indent + "  ");
      }
      std::cout << indent << ")";
    }
    std::cout << std::endl;
  }

  static VCPCapabilityElement ParseToken(Token const& token)
  {
    VCPCapabilityElement element{};
    element.Value = token.Value;
    if (token.Value.size() == 2 && std::isxdigit(token.Value.at(0)) && std::isxdigit(token.Value.at(1)))
    {
      std::stringstream ss;
      int byte;
      ss << std::hex << token.Value;
      ss >> byte;
      element.Value = byte;
      element.ValueType = VCPCapabilityValueType::VCPCode;
    }
    else
    {
      element.Value = token.Value;
      element.ValueType = VCPCapabilityValueType::Text;
    }
    if (token.Children.empty())
    {
      element.ElementType = VCPCapabilityElementType::Leaf;
    }
    else
    {
      element.ElementType = VCPCapabilityElementType::Tree;
      for (const auto& child : token.Children)
      {
        element.Children.push_back(ParseToken(child));
      }
    }
    return element;
  }

  static std::vector<VCPCapabilityElement> ParseLowLevelCapabilitiesString(const std::string& capabilities)
  {
    std::vector<VCPCapabilityElement> elements;
    const auto tokens = TokenizeLowLevelCapabilitiesString(capabilities);
    for (const auto& token : tokens)
    {
      elements.push_back(ParseToken(token));
    }
    return elements;
  }
};




void PrintInfo(MonitorUtils::Monitor const& monitor)
{
  const auto info = MonitorUtils::GetMonitorInfoA(monitor);
  std::cout << "Monitor Info" << std::endl;
  std::cout << "------------" << std::endl;
  std::cout << "Name: " << info.Name << std::endl;
  std::cout << "Primary: " << ((info.Primary) ? "true" : "false") << std::endl;
}

void PrintHighLevelCapabilities(MonitorUtils::Monitor const& monitor)
{
  const auto highLevelCapabilities = MonitorUtils::GetHighLevelCapabilities(monitor);
  std::cout << "High-level capabilities:" << std::endl;
  if (highLevelCapabilities.Valid)
  {
    MonitorUtils::Print(highLevelCapabilities, "  ");
  }
  else
  {
    std::cerr<< "Could not obtain high-level capabililties." << std::endl;
  }
}

void PrintLowLevelCapabilities(MonitorUtils::Monitor const& monitor)
{
  const auto lowLevelCapabilties = MonitorUtils::GetLowLevelCapabilities(monitor);
  std::cout << "Low-level capabilities:" << std::endl;
  if (lowLevelCapabilties.Valid)
  {
    MonitorUtils::Print(lowLevelCapabilties.Capabilities, "  ");
  }
  else
  {
    std::cerr << "Could not obtain low-level capabilities." << std::endl;
    PrintLastError();
  }
}

MonitorUtils::VCPFeatureResult Verify(MonitorUtils::Monitor const& monitor, uint32_t vcpCode, uint32_t vcpValue)
{
  const auto startTime = std::chrono::steady_clock::now();
  MonitorUtils::VCPFeatureResult result;
  result.Success = false;
  while (std::chrono::steady_clock::now() - startTime < std::chrono::milliseconds{ 3000 })
  {
    result = MonitorUtils::GetVCPFeature(monitor, vcpCode);
    if (result.Success && result.CurrentValue == vcpValue)
    {
      break;
    }
  }
  return result;
}

void PrintCapabilities(MonitorUtils::Monitor const& monitor)
{
  PrintHighLevelCapabilities(monitor);
  PrintLowLevelCapabilities(monitor);
}

bool Toggle(MonitorUtils::Monitor const& monitor, bool verify = false)
{
  const auto inputSourceCode = 0x60;
  const auto result = MonitorUtils::GetVCPFeature(monitor, inputSourceCode);
  if (result.Success)
  {
    const auto inputSourceHdmi = 0x11;
    const auto inputSourceDisplayPort = 0xF;
    const auto toggledInputSource = (result.CurrentValue == inputSourceHdmi) ? inputSourceDisplayPort : inputSourceHdmi;
    if (MonitorUtils::SetVCPFeature(monitor, inputSourceCode, toggledInputSource))
    {
      if (verify)
      {
        const auto result = Verify(monitor, inputSourceCode, toggledInputSource);
        return result.Success;
      }
      else
      {
        return true;
      }
    }
  }
  return false;
}

struct Arguments
{
  bool Valid = { false };
  bool MonitorIndex = 0;
  bool PrintInfo{ false };
  bool PrintCapabilities{ false };
  bool SetVCPFeature{ false };
  uint32_t SetVCPFeatureAddress{ 0x0 };
  uint32_t SetVCPFeatureValue{ 0x0 };
  bool GetVCPFeature{ false };
  uint32_t GetVCPFeatureAddress{ 0x0 };
  bool Verify{ false };
  bool Toggle{ false };
};

std::vector<std::string> TokenizeArguments(int argc, char** argv)
{
  std::vector<std::string> args;
  for (auto i = 1; i < argc; ++i)
  {
    args.emplace_back(argv[i]);
  }
  return args;
}

bool ICompare(std::string const& a, std::string const& b)
{
  return _stricmp(a.c_str(), b.c_str()) == 0;
}

template<typename T>
bool GetHex(std::string const& s, T* out)
{
  std::stringstream ss{ s };
  ss >> std::hex >> *out;
  return ss.failbit;
}

template<typename T>
bool Get(std::string const& s, T* out)
{

  if (s.size() > 2 && s.at(0) == '0' && (s.at(1) == 'x' || s.at(1) == 'X'))
  {
    return GetHex(s.substr(2), out);
  }
  else
  {
    std::stringstream ss{ s };
    ss >> *out;
    return ss.failbit;
  }
}

Arguments ParseArguments(std::vector<std::string> const& args)
{
  Arguments arguments{};
  arguments.Valid = true;

  for (auto i = 0u; i < args.size(); ++i)
  {
    auto arg = args.at(i);
    
    if (ICompare("--monitor", arg) || ICompare("-m", arg))
    {
      ++i;
      if (i == args.size())
      {
        std::cerr << "--monitor/-m requires a monitor index" << std::endl;
        arguments.Valid = false;
        break;
      }
      arg = args.at(i);
      if (!Get(arg, &arguments.MonitorIndex))
      {
        std::cerr << "Expected a monitor index, but got: " << arg << std::endl;
        arguments.Valid = false;
        break;
      }
    }
    else if (ICompare("--info", arg) || ICompare("-i", arg))
    {
      arguments.PrintInfo = true;
    }
    else if (ICompare("--capabilities", arg) || ICompare("-c", arg))
    {
      arguments.PrintCapabilities = true;
    }
    else if (ICompare("--get", arg) || ICompare("-g", arg))
    {
      arguments.GetVCPFeature = true;
      ++i;
      if (i == args.size())
      {
        std::cerr << "--get/-g requires an address" << std::endl;
        arguments.Valid = false;
        break;
      }
      arg = args.at(i);
      if (!Get(arg, &arguments.GetVCPFeatureAddress))
      {
        std::cerr << "Expected an address, but got: " << arg << std::endl;
        arguments.Valid = false;
        break;
      }
      arguments.GetVCPFeatureAddress &= 0xFF;
    }
    else if (ICompare("--set", arg) || ICompare("-s", arg))
    {
      arguments.SetVCPFeature = true;
      ++i;
      if (i == args.size())
      {
        std::cerr << "--set/-s requires an address and value" << std::endl;
        arguments.Valid = false;
        break;
      }
      arg = args.at(i);
      if (!Get(arg, &arguments.SetVCPFeatureAddress))
      {
        std::cerr << "Expected an address, but got: " << arg << std::endl;
        arguments.Valid = false;
        break;
      }
      arguments.SetVCPFeatureAddress &= 0xFF;
      ++i;
      if (i == args.size())
      {
        std::cerr << "--set/-s requires an address and value" << std::endl;
        arguments.Valid = false;
        break;
      }
      arg = args.at(i);
      if (!Get(arg, &arguments.SetVCPFeatureValue))
      {
        std::cerr << "Expected an value, but got: " << arg << std::endl;
        arguments.Valid = false;
        break;
      }
    }
    else if (ICompare("--verify", arg) || ICompare("-v", arg))
    {
      arguments.Verify = true;
    }
    else if (ICompare("--toggle", arg))
    {
      arguments.Toggle = true;
    }
    else
    {
      std::cerr << "Unsupported argument: " << arg << std::endl;
      arguments.Valid = false;
      break;
    }
  }

  // Post validation
  if (arguments.GetVCPFeature && arguments.SetVCPFeature)
  {
    std::cerr << "You cannot specify both get and set operations in a single command" << std::endl;
    arguments.Valid = false;
  }

  return arguments;
}

void PrintUsage()
{
  std::cout << "monitor_util [--monitor/-m INDEX] [--info/-i] [--capabilities/-c] [(--get/-g ADDRESS) | (--set/-s ADDRESS VALUE ) | (--toggle)] [--verify/-v]" << std::endl;
}

int main(int argc, char** argv)
{
  const auto argTokens = TokenizeArguments(argc, argv);
  const auto args = ParseArguments(argTokens);
  if (!args.Valid)
  {
    PrintUsage();
    return 1;
  }

  const auto monitor = MonitorUtils::GetMonitor(args.MonitorIndex);

  //while (!IsDebuggerPresent())
  //{
  //  std::this_thread::yield();
  //}

  if (monitor.GetHandle())
  {
    const auto currentInputSourceResult = MonitorUtils::GetVCPFeature(monitor, 0x60);
    if (args.PrintInfo)
    {
      PrintInfo(monitor);
    }
    if (args.PrintCapabilities)
    {
      PrintCapabilities(monitor);
    }

    if (args.GetVCPFeature)
    {
      const auto result = MonitorUtils::GetVCPFeature(monitor, args.GetVCPFeatureAddress);
      if (result.Success)
      {
        std::cout << "VCP feature 0x" << std::hex << args.GetVCPFeatureAddress << " = 0x" << result.CurrentValue << std::endl;
      }
      else
      {
        std::cerr << "Failed to read VCP feature 0x" << std::hex << args.GetVCPFeatureAddress << std::endl;
      }
    }
    else if (args.SetVCPFeature)
    {
      if (MonitorUtils::SetVCPFeature(monitor, args.SetVCPFeatureAddress, args.SetVCPFeatureValue))
      {
        std::cout << "Setting VCP feature 0x" << std::hex << args.SetVCPFeatureAddress << " = 0x" << args.SetVCPFeatureValue << std::endl;
        if (args.Verify)
        {
          const auto result = Verify(monitor, args.SetVCPFeatureAddress, args.SetVCPFeatureValue);
          if (result.Success)
          {
            if (result.CurrentValue == args.SetVCPFeatureValue)
            {
              std::cout << "Success" << std::endl;
            }
            else
            {
              std::cerr << "Failed to verify - expected 0x" << std::hex << args.SetVCPFeatureValue << ", but got 0x" << result.CurrentValue << std::endl;
            }
          }
          else
          {
            std::cerr << "Failed to verify - read-back failed." << std::endl;
          }
        }
        else
        {
          std::cout << "Success" << std::endl;
        }
      }
      else
      {
        std::cerr << "Failure - failed to set value" << std::endl;
      }
    }
    else if (args.Toggle)
    {
      if (Toggle(monitor, args.Verify))
      {
        std::cout << "Successfully toggled input source" << std::endl;
      }
      else
      {
        std::cerr << "Failed to toggle input source" << std::endl;
      }
    }
    
    //std::cout << "Current input source: ";
    //if (currentInputSourceResult.Success)
    //{
    //  std::cout << "0x" << std::hex << currentInputSourceResult.CurrentValue << std::endl;
    //}
    //else
    //{
    //  std::cerr << "Could not obtain VCP setting for 0x" << std::hex << 0x60 << std::endl;
    //}
    //MonitorUtils::SetVCPFeature(monitor, 0x60, 0x0f); // 0x0F = DisplayPort, 0x11 = HDMI
  }
  else
  {
    std::cerr << "Failed to get monitor handle" << std::endl;
    PrintLastError();
  }
 }