# Pidgin Groupchat Typing-Notifications
Adds typing notifications for multi-user group chats in Pidgin


![capture_1474610788](https://cloud.githubusercontent.com/assets/1063865/18775850/edb82350-81b8-11e6-8d08-9067538104cf.png)


Currently only tested as working with the Hangouts plugin, but support for other protocols to come later


## For developers
This plugin relies on the `PURPLE_CBFLAGS_TYPING` flag being set with `purple_conv_chat_user_set_flags()` when a user is typing
