#ifndef SCREEN_COMMAND_H
#define SCREEN_COMMAND_H
#include <string>

enum class ScreenType
{
    IVI,
    Cluster,
    Ceiling,
    Unknown
};

struct  ScreenCommand
{
    enum class Type {
        Unknown,
        GetSver,
        GetHver,
        GetBlver,
        GetTpver,
        UpgradeP,
        UpgradeTp,
    };
    Type type{Type::Unknown};
    bool isUpgrade{false};
    int replyFd{-1};
    ScreenType screenType{ScreenType::Unknown};
    bool valid{false};
    std::string finalCmd;
    std::string binPath;
    
    static ScreenCommand parse(const std::string& rawCmd,int replyFd, ScreenType screenType);
};
#endif  //SCREEN_COMMAND_H
