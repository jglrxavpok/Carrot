//
// Created by jglrxavpok on 25/12/2023.
//

#include "UserNotifications.h"
#include "Assert.h"
#include "stringmanip.h"

namespace Carrot {
    UserNotifications& UserNotifications::getInstance() {
        static UserNotifications instance;
        return instance;
    }

    NotificationID UserNotifications::showNotification(Carrot::NotificationDescription&& desc) {
        NotificationID id = notificationIDCounter++;

        NotificationState* pNotif = nullptr;
        {
            Carrot::Async::LockGuard g { notificationsAccess };
            auto [iter, wasNew] = notifications.try_emplace(id);
            verify(wasNew, "Added notification with an ID which already existed?");
            pNotif = &iter->second;
        }

        pNotif->title = std::move(desc.title);
        pNotif->body = std::move(desc.body);
        return id;
    }

    void UserNotifications::closeNotification(NotificationID notificationID) {
        Carrot::Async::LockGuard g { notificationsAccess };
        std::size_t removedCount = notifications.erase(notificationID);
        verify(removedCount == 1, Carrot::sprintf("No notification matched with ID %llu", notificationID));
    }

    void UserNotifications::setProgress(NotificationID notificationID, float progress) {
        Carrot::Async::LockGuard g { notificationsAccess };
        NotificationState& notification = notifications.at(notificationID);
        notification.progress = progress;
    }

    void UserNotifications::setBody(NotificationID notificationID, std::string&& text) {
        Carrot::Async::LockGuard g { notificationsAccess };
        NotificationState& notification = notifications.at(notificationID);
        notification.body = std::move(text);
    }

    void UserNotifications::getNotifications(std::vector<NotificationState>& toFill) {
        Carrot::Async::LockGuard g { notificationsAccess };
        if(toFill.capacity() < toFill.size() + notifications.size()) {
            toFill.reserve(toFill.size() + notifications.size());
        }
        for(auto& [k, notif] : notifications) {
            toFill.emplace_back(notif);
        }
    }
} // Carrot