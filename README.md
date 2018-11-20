# Pidgin Groupchat Typing-Notifications
Adds typing notifications for multi-user group chats in Pidgin


![capture_1474610788](https://cloud.githubusercontent.com/assets/1063865/18775850/edb82350-81b8-11e6-8d08-9067538104cf.png)


Works with XMPP MUC's out of the box. Also works with the Hangouts plugin, Skype plugin, Matrix plugin, Telegram and more.


## For prpl developers
(Refer to the baked-in XMPP support [as an example](https://github.com/EionRobb/pidgin-groupchat-typing-notifications/blob/8b3e234f9aa7016216ee91e008030b3bfe45612c/grouptyping.c#L289-L376))

This plugin relies on the `PURPLE_CBFLAGS_TYPING` flag being set with `purple_conv_chat_user_set_flags()` when a user is typing to display.

To listen to the event when the UI is typing in a group chat, attach to the `chat-conversation-typing`  signal on the Purple conversations handle when logging in.  Due to a race for when the group typing plugin is loaded and protocol plugins are loaded, you should only attach to the signal during the `_login` stage, but only once so that you don't receive multiple typing notifications, eg:

```C
static gulong chat_conversation_typing_signal = 0;
static void prpl_login(PurpleAccount *account) {
  // ...
  // Your normal login code
  // ...
  
	if (!chat_conversation_typing_signal) {
		chat_conversation_typing_signal = purple_signal_connect(purple_conversations_get_handle(), "chat-conversation-typing", purple_connection_get_protocol(pc), PURPLE_CALLBACK(prpl_conv_send_typing), NULL);
	}
  // ...
}

guint
prpl_conv_send_typing(PurpleConversation *conv, PurpleIMTypingState state, gpointer *handle)
{
	PurpleConnection *pc;
	
	pc = purple_conversation_get_connection(conv);
	
	if (!PURPLE_CONNECTION_IS_CONNECTED(pc))
		return 0;
	
	if (g_strcmp0(purple_protocol_get_id(purple_connection_get_protocol(pc)), "prpl-your-prpl-id"))
		return 0;
  
  // Send typing over protocol
  
  // Return the timeout like you would for your prpl_send_typing()
  return 20;
}

```
