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
    }
    virtual void customSetup() {
        AccountLayer::customSetup();
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto toggleOn  = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
        auto toggleOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
        auto toggle = CCMenuItemToggler::create(toggleOff, toggleOn, this, menu_selector(MyAccountLayer::onCloseToggle));
        toggle->setScale(0.7f);
        toggle->toggle(false);
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

class PostSavePopup : public geode::Popup {
protected:
    std::function<void(PostSaveAction)> m_callback;
    PostSaveAction m_selectedAction = PostSaveAction::NOTHING;
    std::vector<CCMenuItemToggler*> m_radioToggles;
    
    bool init(std::function<void(PostSaveAction)> callback) {
        if (!Popup::init(280.f, 240.f))
            return false;
        
        m_callback = callback;
        this->setTitle("After Save Action");
        auto winSize = CCSize{280.f, 240.f};
        std::vector<std::pair<std::string, PostSaveAction>> options = {
            {"Do Nothing", PostSaveAction::NOTHING},
            {"Close GD", PostSaveAction::CLOSE_GD},
            {"Shutdown PC", PostSaveAction::SHUTDOWN_PC},
            {"Restart PC", PostSaveAction::RESTART_PC},
            {"Sleep PC", PostSaveAction::SLEEP_PC},
            {"Close GD + Sleep PC", PostSaveAction::CLOSE_AND_SLEEP}
        };
        float startY = winSize.height / 2 + 30.f;
        for (size_t i = 0; i < options.size(); i++) {
            auto& [text, action] = options[i];
            auto toggleOn = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
            auto toggleOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
            auto toggle = CCMenuItemToggler::create(toggleOff, toggleOn, this, menu_selector(PostSavePopup::onRadioToggle));
            toggle->setScale(0.6f);
            toggle->setTag(static_cast<int>(action));
            toggle->toggle(action == PostSaveAction::NOTHING);
            auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
            label->setScale(0.45f);
            label->setAnchorPoint({0.f, 0.5f});
            auto menu = CCMenu::create();
            menu->setPosition(ccp(winSize.width / 2 - 80.f, startY - i * 28.f));
            menu->setContentSize({200.f, 25.f});
            toggle->setPosition(ccp(0.f, 0.f));
            menu->addChild(toggle);
            label->setPosition(ccp(20.f, 0.f));
            menu->addChild(label);
            m_mainLayer->addChild(menu);
            m_radioToggles.push_back(toggle);
        }
        auto okSprite = ButtonSprite::create("OK", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        auto okBtn = CCMenuItemSpriteExtra::create(okSprite, this, menu_selector(PostSavePopup::onOK));
        auto btnMenu = CCMenu::create();
        btnMenu->addChild(okBtn);
        btnMenu->setPosition(winSize.width / 2, 25.f);
        m_mainLayer->addChild(btnMenu);
        return true;
    }
    void onRadioToggle(CCObject* sender) {
        auto clicked = static_cast<CCMenuItemToggler*>(sender);
        m_selectedAction = static_cast<PostSaveAction>(clicked->getTag());
        for (auto toggle : m_radioToggles) {
            toggle->toggle(static_cast<PostSaveAction>(toggle->getTag()) == m_selectedAction);
        }
    }
    void onOK(CCObject*) {
        if (m_callback) m_callback(m_selectedAction);
        this->onClose(nullptr);
    }
public:
    static PostSavePopup* create(std::function<void(PostSaveAction)> callback) {
        auto ret = new PostSavePopup();
        if (ret->init(callback)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

class $modify(MyAccountLayer, AccountLayer) {
    struct Fields {
        PostSaveAction m_postSaveAction = PostSaveAction::NOTHING;
        CCLabelBMFont* m_statusLabel = nullptr;
        bool m_hasShownWarning = false;
    };
    void onActionButton(CCObject*) {
        PostSavePopup::create([this](PostSaveAction action) {
            m_fields->m_postSaveAction = action;
            updateStatusLabel();
        })->show();
    }
    void updateStatusLabel() {
        if (!m_fields->m_statusLabel) return;
        std::string text;
        switch (m_fields->m_postSaveAction) {
            case PostSaveAction::NOTHING: text = "Action: None"; break;
            case PostSaveAction::CLOSE_GD: text = "Action: Close GD"; break;
            case PostSaveAction::SHUTDOWN_PC: text = "Action: Shutdown PC"; break;
            case PostSaveAction::RESTART_PC: text = "Action: Restart PC"; break;
            case PostSaveAction::SLEEP_PC: text = "Action: Sleep PC"; break;
            case PostSaveAction::CLOSE_AND_SLEEP: text = "Action: Close + Sleep"; break;
        }
        m_fields->m_statusLabel->setString(text.c_str());
    }
    virtual void customSetup() {
        AccountLayer::customSetup();
        
        if (!m_fields->m_hasShownWarning && Loader::get()->isModLoaded("beefyandtheducks.save-on-exit")) {
            m_fields->m_hasShownWarning = true;
            FLAlertLayer::create("Warning", "Save On Exit by beefyandtheducks is loaded, which if game closed it'll save TWICE once by me and once by him, so be careful", "OK")->show();
        }
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto buttonSprite = ButtonSprite::create("After Save...", "goldFont.fnt", "GJ_button_04.png", 0.8f);
        buttonSprite->setScale(0.7f);
        auto button = CCMenuItemSpriteExtra::create(buttonSprite, this, menu_selector(MyAccountLayer::onActionButton));
        auto statusLabel = CCLabelBMFont::create("Action: None", "bigFont.fnt");
        statusLabel->setScale(0.3f);
        m_fields->m_statusLabel = statusLabel;
        auto menu = CCMenu::create();
        menu->setPosition(ccp(180.5f, 227.f));
        menu->setContentSize({250.f, 50.f});
        button->setPosition(ccp(0.f, 10.f));
        menu->addChild(button);
        statusLabel->setPosition(ccp(0.f, -10.f));
        menu->addChild(statusLabel);
        this->addChild(menu, 10);
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
            if (std::getenv("WINEPREFIX")) {
                system("systemctl suspend");
            } else {
                system("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
            }
#elif defined(GEODE_IS_MACOS)
            system("pmset sleepnow");
#endif
            return; // dont close gd
        } else if (action == PostSaveAction::CLOSE_AND_SLEEP) {
#ifdef GEODE_IS_WINDOWS
            if (std::getenv("WINEPREFIX")) {
                system("systemctl suspend &");
            } else {
                system("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
            }
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
