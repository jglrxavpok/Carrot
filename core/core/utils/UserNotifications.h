//
// Created by jglrxavpok on 25/12/2023.
//

#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <core/async/Locks.h>

namespace Carrot {

    using NotificationID = std::uint64_t;

    struct NotificationDescription {
        std::string title;
        std::string body;
        // TODO: icon?
    };

    struct NotificationState {
        std::string title;
        std::string body;
        float progress = -1.0f;
    };

    class UserNotifications {
    public:
        static UserNotifications& getInstance();

        /**
         * Starts showing the given notification, and returns the ID of the notification
         * (used to reference to this notification in other methods)
         */
        NotificationID showNotification(NotificationDescription&& desc);

        /**
         * Stop showing the given notification, and delete the related resources.
         * @param notificationID notification to remove
         */
        void closeNotification(NotificationID notificationID);

        /**
         * Sets the progress value of the given notification.
         *
         * @param notificationID ID of the notification to change progress value
         * @param progress value of progress. Between 0 and 1.0 to show a progress bar, &lt; 0 to show a "indeterminate" progress bar (displays animation to show still working but no progress value known)
         */
        void setProgress(NotificationID notificationID, float progress);

        /**
         * Sets the body text of the given notification.
         *
         * @param notificationID ID of the notification to change progress value
         * @param progress text to show. Can be empty
         */
        void setBody(NotificationID notificationID, std::string&& text);

        /**
         * Fills the given vector with the currently active notifications. This is a COPY of the current state, the states inside the vector will not change
         * when the notification is modified via a method of UserNotifications
         * @param notifications vector to add currently active notifications to. The vector is not cleared at the start, and notifications are appended at its back
         */
        void getNotifications(std::vector<NotificationState>& notifications);

    private:
        Carrot::Async::SpinLock notificationsAccess;
        std::atomic<NotificationID> notificationIDCounter {0};
        std::unordered_map<NotificationID, NotificationState> notifications;
    };

} // Carrot
