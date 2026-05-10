#include <Geode/Geode.hpp>
#include <Geode/modify/AccountLayer.hpp> // my fucking bad

using namespace geode::prelude;

class $modify(MyAccountLayer, AccountLayer) {
    struct Fields {
        bool m_closeOnSave = false;
        CCMenuItemToggler* m_closeToggle = nullptr;
    };
    void onCloseToggle(CCObject* sender) {
        m_fields->m_closeOnSave = !m_fields->m_closeOnSave;
        log::info("Close on save toggled: {}", m_fields->m_closeOnSave);
    }
    virtual void customSetup() {
        AccountLayer::customSetup();
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto toggleOn  = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
        auto toggleOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
        auto toggle = CCMenuItemToggler::create(
            toggleOff,
            toggleOn,
            this,
            menu_selector(MyAccountLayer::onCloseToggle)
        );
        toggle->setScale(0.7f);
        toggle->toggle(false);
        m_fields->m_closeToggle = toggle;
        auto label = CCLabelBMFont::create("Close Game after successful save?", "bigFont.fnt");
        label->setScale(0.35f);
        label->setAnchorPoint({ 0.f, 0.5f });
        auto menu = CCMenu::create();
        menu->setPosition(ccp(winSize.width / 2 - 120.f, 38.f));
        menu->setContentSize({ 250.f, 30.f });
        toggle->setPosition(ccp(0.f, 0.f));
        menu->addChild(toggle);
        label->setPosition(ccp(14.f, 0.f));
        menu->addChild(label);
        // add to layer
        this->addChild(menu, 10);
    }
    virtual void backupAccountFinished() {
        AccountLayer::backupAccountFinished();
        if (m_fields->m_closeOnSave) {
            log::info("Backup successful, closing game as requested.");
            // delay
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
            log::info("Sync successful, closing game as requested.");
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
#if defined(GEODE_IS_IOS) || defined(GEODE_IS_MACOS)
        exit(0); // my gf told me about it since shes an iphoner
#endif
    }
};
