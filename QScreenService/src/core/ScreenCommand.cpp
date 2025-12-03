#include "../../inc/core/ScreenCommand.h"
#include <string>
#include <algorithm>

constexpr const char* lcdSverCmd{"lcdgetsuppliersver"};
constexpr const char* lcdHverCmd{"lcdgethver"};
constexpr const char* lcdBlverCmd{"lcdgetblver"};
constexpr const char* lcdTpverCmd{"lcdgettpver"};
constexpr const char* lcdUpdateCmd{"lcdupdate-p"};
constexpr const char* lcdUpdateTpCmd{"lcdupdatetp-p"};

using namespace std;
static  inline std::string trim(const std::string& str)
{
    size_t first = str.find_first_not_of("\t\r\n");
    if(first == std::string::npos)
        return "";
    size_t last = str.find_last_not_of("\t\r\n");
    return str.substr(first, last - first + 1);
}

ScreenCommand ScreenCommand::parse(const std::string& rawCmd,int replyFd, ScreenType screenType)
{
    ScreenCommand cmd;
    cmd.finalCmd = trim(rawCmd);
    cmd.replyFd = replyFd;
    cmd.screenType = screenType;
    std::string lower = cmd.finalCmd;
    lower.erase(std::remove(lower.begin(), lower.end(), ' '), lower.end());
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });
    cmd.finalCmd = lower;
    if(lower.find(lcdSverCmd)!=std::string::npos) {
        cmd.type = Type::GetSver;
        cmd.valid = true;
        return cmd;
    } else if(lower.find(lcdHverCmd)!=std::string::npos){
        cmd.type = Type::GetHver;
        cmd.valid = true;
        return cmd;
    }else if(lower.find(lcdBlverCmd)!=std::string::npos) {
        cmd.type = Type::GetBlver;
        cmd.valid = true;
        return cmd;
    } else if(lower.find(lcdTpverCmd)!=std::string::npos) {
        cmd.type = Type::GetTpver;
        cmd.valid = true;
        return cmd;
    } else if(lower.find(lcdUpdateCmd)!=std::string::npos) {
        cmd.type = Type::UpgradeP;
        cmd.isUpgrade = true;
    } else if(lower.find(lcdUpdateTpCmd)!=std::string::npos) {
        cmd.type = Type::UpgradeTp;
        cmd.isUpgrade = true;
    } else {
        cmd.type = Type::Unknown;
    }
    
    if(cmd.isUpgrade){
        size_t p = lower.find("-p");
        if(p != std::string::npos){
            cmd.binPath = trim(lower.substr(p+2));
        }
        cmd.valid = !cmd.binPath.empty();
        return cmd;
    }
    cmd.valid = false;
    return cmd;
}
