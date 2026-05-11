#include <Geode/Geode.hpp>
#include <Geode/modify/AccountLayer.hpp> // my fucking bad

using namespace geode::prelude;

#if defined(GEODE_IS_ANDROID) || defined(GEODE_IS_IOS)

class $modify(MyAccountLayer, AccountLayer) {
    struct Fields {
        bool m_closeOnSave = false;
        CCMenuItemToggler* m_closeToggle = nullptr;
    };
    void onCloseToggle(CCObject* sender) {
        m_fields->m_closeOnSave = !m_fields->m_closeOnSave;
        Mod::get()->setSavedValue("close-on-save", m_fields->m_closeOnSave);
    }
    virtual void customSetup() {
        AccountLayer::customSetup();
        m_fields->m_closeOnSave = Mod::get()->getSavedValue<bool>("close-on-save", false);
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto toggleOn  = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
        auto toggleOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
        auto toggle = CCMenuItemToggler::create(toggleOff, toggleOn, this, menu_selector(MyAccountLayer::onCloseToggle));
        toggle->setScale(0.7f);
        toggle->toggle(m_fields->m_closeOnSave);
        m_fields->m_closeToggle = toggle;
        auto label = CCLabelBMFont::create("close GD after successful save", "bigFont.fnt");
        label->setScale(0.35f);
        label->setAnchorPoint({ 0.f, 0.5f });
        auto menu = CCMenu::create();
        menu->setPosition(ccp(180.5f, 227.f));
        menu->setContentSize({ 250.f, 30.f });
        toggle->setPosition(ccp(0.f, 0.f));
        menu->addChild(toggle);
        label->setPosition(ccp(14.f, 0.f));
        menu->addChild(label);
        this->addChild(menu, 10);
    }
    virtual void backupAccountFinished() {
        AccountLayer::backupAccountFinished();
        if (m_fields->m_closeOnSave) {
            this->runAction(CCSequence::create(
                CCDelayTime::create(0.8f),
                CCCallFunc::create(this, callfunc_selector(MyAccountLayer::doQuitGame)),
                nullptr
            ));
        }
    }
    virtual void syncAccountFinished() {
        AccountLayer::syncAccountFinished();
        if (m_fields->m_closeOnSave) {
            this->runAction(CCSequence::create(
                CCDelayTime::create(0.8f),
                CCCallFunc::create(this, callfunc_selector(MyAccountLayer::doQuitGame)),
                nullptr
            ));
        }
    }
    void doQuitGame() {
        GameManager::sharedState()->save();
        LocalLevelManager::sharedState()->save();
        CCDirector::sharedDirector()->end();
#ifdef GEODE_IS_IOS
        exit(0); // my gf told me about it since shes an iphoner
#endif
    }
};

#else

enum class PostSaveAction {
    NOTHING, CLOSE_GD, SHUTDOWN_PC, RESTART_PC, SLEEP_PC, CLOSE_AND_SLEEP
};

class $modify(MyAccountLayer, AccountLayer) {
    struct Fields {
        PostSaveAction m_postSaveAction = PostSaveAction::NOTHING;
        CCLabelBMFont* m_statusLabel = nullptr;
        bool m_hasShownWarning = false;
        bool m_isWine = false;
    };
    
    void checkWineAndLoadAction() {
#ifdef GEODE_IS_WINDOWS
        if (std::getenv("WINEPREFIX")) {
            m_fields->m_isWine = true;
            m_fields->m_postSaveAction = Mod::get()->getSavedValue<bool>("close-on-save", false) ? PostSaveAction::CLOSE_GD : PostSaveAction::NOTHING;
        } else {
            m_fields->m_isWine = false;
            auto actionStr = Mod::get()->getSavedValue<std::string>("post-save-action", "nothing");
            if (actionStr == "close") m_fields->m_postSaveAction = PostSaveAction::CLOSE_GD;
            else if (actionStr == "shutdown") m_fields->m_postSaveAction = PostSaveAction::SHUTDOWN_PC;
            else if (actionStr == "restart") m_fields->m_postSaveAction = PostSaveAction::RESTART_PC;
            else if (actionStr == "sleep") m_fields->m_postSaveAction = PostSaveAction::SLEEP_PC;
            else if (actionStr == "close-and-sleep") m_fields->m_postSaveAction = PostSaveAction::CLOSE_AND_SLEEP;
            else m_fields->m_postSaveAction = PostSaveAction::NOTHING;
        }
#elif defined(GEODE_IS_MACOS)
        m_fields->m_isWine = false;
        auto actionStr = Mod::get()->getSavedValue<std::string>("post-save-action", "nothing");
        if (actionStr == "close") m_fields->m_postSaveAction = PostSaveAction::CLOSE_GD;
        else if (actionStr == "shutdown") m_fields->m_postSaveAction = PostSaveAction::SHUTDOWN_PC;
        else if (actionStr == "restart") m_fields->m_postSaveAction = PostSaveAction::RESTART_PC;
        else if (actionStr == "sleep") m_fields->m_postSaveAction = PostSaveAction::SLEEP_PC;
        else if (actionStr == "close-and-sleep") m_fields->m_postSaveAction = PostSaveAction::CLOSE_AND_SLEEP;
        else m_fields->m_postSaveAction = PostSaveAction::NOTHING;
#endif
    }
    
    void onCloseToggle(CCObject* sender) {
        bool newValue = !Mod::get()->getSavedValue<bool>("close-on-save", false);
        Mod::get()->setSavedValue("close-on-save", newValue);
        m_fields->m_postSaveAction = newValue ? PostSaveAction::CLOSE_GD : PostSaveAction::NOTHING;
        updateStatusLabel();
    }
    
    void updateStatusLabel() {
        if (!m_fields->m_statusLabel) return;
        std::string text;
        if (m_fields->m_isWine) {
            text = Mod::get()->getSavedValue<bool>("close-on-save", false) ? "Will close GD" : "Won't close GD";
        } else {
            switch (m_fields->m_postSaveAction) {
                case PostSaveAction::NOTHING: text = "Action: None"; break;
                case PostSaveAction::CLOSE_GD: text = "Action: Close GD"; break;
                case PostSaveAction::SHUTDOWN_PC: text = "Action: Shutdown PC"; break;
                case PostSaveAction::RESTART_PC: text = "Action: Restart PC"; break;
                case PostSaveAction::SLEEP_PC: text = "Action: Sleep PC"; break;
                case PostSaveAction::CLOSE_AND_SLEEP: text = "Action: Close + Sleep"; break;
            }
        }
        m_fields->m_statusLabel->setString(text.c_str());
    }
    
    virtual void customSetup() {
        AccountLayer::customSetup();
        
        checkWineAndLoadAction();
        
        if (!m_fields->m_hasShownWarning && Loader::get()->isModLoaded("beefyandtheducks.save-on-exit")) {
            m_fields->m_hasShownWarning = true;
            FLAlertLayer::create("Warning", "Save On Exit by beefyandtheducks is loaded, which if game closed it'll save TWICE once by me and once by him, so be careful", "OK")->show();
        }
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        
        if (m_fields->m_isWine) {
            // Wine: show simple toggle like mobile
            auto toggleOn  = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
            auto toggleOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
            auto toggle = CCMenuItemToggler::create(toggleOff, toggleOn, this, menu_selector(MyAccountLayer::onCloseToggle));
            toggle->setScale(0.7f);
            toggle->toggle(Mod::get()->getSavedValue<bool>("close-on-save", false));
            auto label = CCLabelBMFont::create("close GD after successful save", "bigFont.fnt");
            label->setScale(0.35f);
            label->setAnchorPoint({ 0.f, 0.5f });
            auto menu = CCMenu::create();
            menu->setPosition(ccp(180.5f, 227.f));
            menu->setContentSize({ 250.f, 30.f });
            toggle->setPosition(ccp(0.f, 0.f));
            menu->addChild(toggle);
            label->setPosition(ccp(14.f, 0.f));
            menu->addChild(label);
            this->addChild(menu, 10);
        } else {
            // Native Windows/macOS: show status label
            auto statusLabel = CCLabelBMFont::create("", "bigFont.fnt");
            statusLabel->setScale(0.3f);
            m_fields->m_statusLabel = statusLabel;
            auto menu = CCMenu::create();
            menu->setPosition(ccp(283.5f, 217.f));
            menu->setContentSize({250.f, 50.f});
            statusLabel->setPosition(ccp(0.f, 0.f));
            menu->addChild(statusLabel);
            this->addChild(menu, 10);
            updateStatusLabel();
        }
    }
    
    virtual void backupAccountFinished() {
        AccountLayer::backupAccountFinished();
        if (m_fields->m_postSaveAction == PostSaveAction::NOTHING) return;
        this->runAction(CCSequence::create(
            CCDelayTime::create(0.8f),
            CCCallFunc::create(this, callfunc_selector(MyAccountLayer::doAction)),
            nullptr
        ));
    }
    virtual void syncAccountFinished() {
        AccountLayer::syncAccountFinished();
        if (m_fields->m_postSaveAction == PostSaveAction::NOTHING) return;
        this->runAction(CCSequence::create(
            CCDelayTime::create(0.8f),
            CCCallFunc::create(this, callfunc_selector(MyAccountLayer::doAction)),
            nullptr
        ));
    }
    void doAction() {
        auto action = m_fields->m_postSaveAction;
        if (action == PostSaveAction::SHUTDOWN_PC) {
#ifdef GEODE_IS_WINDOWS
            system("shutdown /s /t 4");
#elif defined(GEODE_IS_MACOS)
            system("osascript -e 'tell app \"System Events\" to shut down'");
#endif
        } else if (action == PostSaveAction::RESTART_PC) {
#ifdef GEODE_IS_WINDOWS
            system("shutdown /r /t 4");
#elif defined(GEODE_IS_MACOS)
            system("osascript -e 'tell app \"System Events\" to restart'");
#endif
        } else if (action == PostSaveAction::SLEEP_PC) {
#ifdef GEODE_IS_WINDOWS
            system("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
#elif defined(GEODE_IS_MACOS)
            system("pmset sleepnow");
#endif
            return; // dont close gd
        } else if (action == PostSaveAction::CLOSE_AND_SLEEP) {
#ifdef GEODE_IS_WINDOWS
            system("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
#elif defined(GEODE_IS_MACOS)
            system("pmset sleepnow &");
#endif
        }
        if (action != PostSaveAction::SLEEP_PC) {
            GameManager::sharedState()->save();
            LocalLevelManager::sharedState()->save();
            CCDirector::sharedDirector()->end();
#ifdef GEODE_IS_MACOS
            exit(0);
#endif
        }
    }
};

#endif
