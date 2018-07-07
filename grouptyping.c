
#define PURPLE_PLUGINS

#include "gtkconv.h"
#include "gtkplugin.h"

#ifndef _
#	define _(a) (a)
#	define N_(a) (a)
#endif

#define SEND_TYPED_TIMEOUT_SECONDS 5

static void
update_typing_message(PidginConversation *gtkconv, const char *message)
{
	GtkTextBuffer *buffer;
	GtkTextMark *stmark, *enmark;

	if (g_object_get_data(G_OBJECT(gtkconv->imhtml), "disable-typing-notification"))
		return;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));
	stmark = gtk_text_buffer_get_mark(buffer, "typing-notification-start");
	enmark = gtk_text_buffer_get_mark(buffer, "typing-notification-end");
	if (stmark && enmark) {
		GtkTextIter start, end;
		gtk_text_buffer_get_iter_at_mark(buffer, &start, stmark);
		gtk_text_buffer_get_iter_at_mark(buffer, &end, enmark);
		gtk_text_buffer_delete_mark(buffer, stmark);
		gtk_text_buffer_delete_mark(buffer, enmark);
		gtk_text_buffer_delete(buffer, &start, &end);
	} else if (message && *message == '\n' && message[1] == ' ' && message[2] == '\0')
		message = NULL;

#ifdef RESERVE_LINE
	if (!message)
		message = "\n ";   /* The blank space is required to avoid a GTK+/Pango bug */
#endif

	if (message) {
		GtkTextIter iter;
		gtk_text_buffer_get_end_iter(buffer, &iter);
		gtk_text_buffer_create_mark(buffer, "typing-notification-start", &iter, TRUE);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, message, -1, "TYPING-NOTIFICATION", NULL);
		gtk_text_buffer_get_end_iter(buffer, &iter);
		gtk_text_buffer_create_mark(buffer, "typing-notification-end", &iter, TRUE);
	}
}

static void 
grouptyping_chat_buddy_flags_cb(PurpleConversation *conv, const char *name, PurpleConvChatBuddyFlags oldflags, PurpleConvChatBuddyFlags newflags)
{
	PidginConversation *gtkconv;
	GList *typing_users = NULL;
	gint num_typing = 0;
	gchar *message = NULL;
	
	g_return_if_fail(conv != NULL);
	gtkconv = PIDGIN_CONVERSATION(conv);
	g_return_if_fail(gtkconv != NULL);
	
	if (oldflags & PURPLE_CBFLAGS_TYPING || newflags & PURPLE_CBFLAGS_TYPING) {
		PurpleConvChat *chat = PURPLE_CONV_CHAT(conv);
		GList *chat_users = purple_conv_chat_get_users(chat);
		
		for (; chat_users; chat_users = chat_users->next) {
			PurpleConvChatBuddy *cb = chat_users->data;
			if (cb->flags & PURPLE_CBFLAGS_TYPING) {
				typing_users = g_list_append(typing_users, cb);
				num_typing++;
			}
		}
		
		if (num_typing == 0) {
			//nothing
		} else if (num_typing == 1) {
			PurpleConvChatBuddy *cb = typing_users->data;
			message = g_strdup_printf(_("\n%s is typing..."), cb->alias ? cb->alias : cb->name);
		} else {
			GString *message_str = g_string_new("\n");
			GList *typing_users_itr;
			gint i;
			
			for (typing_users_itr = typing_users, i = 0; i < 3 && i < num_typing; typing_users_itr = typing_users_itr->next, i++) {
				PurpleConvChatBuddy *cb = typing_users_itr->data;
				
				if (i > 0) {
					if (i == num_typing - 1) {
						g_string_append(message_str, " and ");
					} else {
						g_string_append(message_str, ", ");
					}
				}
				g_string_append_printf(message_str, "%s", cb->alias ? cb->alias : cb->name);
			}
			
			if (i >= 3) {
				gint num_others = (num_typing - i);
				g_string_append_printf(message_str, " and %d other%s", num_others, num_others > 1 ? "s" : "");
			}
			g_string_append(message_str, " are typing...");
			
			message = N_(g_string_free(message_str, FALSE));
		}
		
		update_typing_message(gtkconv, message);
		g_free(message);
	}
	
	
	
	// TODO, just do diffs on the list of users
	// if (oldflags & PURPLE_CBFLAGS_TYPING && newflags & ~PURPLE_CBFLAGS_TYPING) {
		// gchar *message;
		
		// message = g_strdup_printf(_("\n%s is typing..."), purple_conversation_get_title(conv));
		// update_typing_message(message);
	// }
}


static time_t
purple_conv_chat_get_type_again(const PurpleConvChat *chat)
{
	g_return_val_if_fail(chat != NULL, 0);

	return GPOINTER_TO_UINT(g_dataset_get_data(chat, "type_again"));
}

static void
purple_conv_chat_set_type_again(PurpleConvChat *chat, unsigned int val)
{
	g_return_if_fail(chat != NULL);

	if (val == 0) {
		g_dataset_set_data(chat, "type_again", NULL);
	} else {
		g_dataset_set_data(chat, "type_again", GUINT_TO_POINTER(time(NULL) + val));
	}
}

static guint
purple_conv_chat_send_typing(PurpleConvChat *chat, PurpleTypingState typing_state)
{
	PurpleConversation *conv = purple_conv_chat_get_conversation(chat);
	gpointer ret_val;
	
	ret_val = purple_signal_emit_return_1(purple_conversations_get_handle(), "chat-conversation-typing", conv, typing_state);
	
	return GPOINTER_TO_UINT(ret_val);
}

static gboolean
send_typed_cb(gpointer data)
{
	PurpleConversation *conv = (PurpleConversation *)data;
	PurpleConvChat *chat;

	g_return_val_if_fail(conv != NULL, FALSE);

	chat = PURPLE_CONV_CHAT(conv);

	if (chat != NULL) {
		/* We set this to 1 so that PURPLE_TYPING will be sent
		 * if the Purple user types anything else.
		 */
		purple_conv_chat_set_type_again(PURPLE_CONV_CHAT(conv), 1);

		purple_conv_chat_send_typing(chat, PURPLE_TYPED);

		//purple_debug_misc("grouptyping", "typed...\n");
	}

	return FALSE;
}

static void
purple_conv_chat_stop_send_typed_timeout(PurpleConvChat *chat)
{
	guint send_typed_timeout;
	
	g_return_if_fail(chat != NULL);
	
	send_typed_timeout = GPOINTER_TO_UINT(g_dataset_get_data(chat, "send_typed_timeout"));
	
	if (send_typed_timeout == 0)
		return;

	purple_timeout_remove(send_typed_timeout);
	g_dataset_set_data(chat, "send_typed_timeout", NULL);
}

static void
purple_conv_chat_start_send_typed_timeout(PurpleConvChat *chat)
{
	guint send_typed_timeout;
	
	g_return_if_fail(chat != NULL);

	send_typed_timeout = purple_timeout_add_seconds(SEND_TYPED_TIMEOUT_SECONDS, send_typed_cb, purple_conv_chat_get_conversation(chat));
	
	g_dataset_set_data(chat, "send_typed_timeout", GUINT_TO_POINTER(send_typed_timeout));
}

static void
got_typing_keypress(PidginConversation *gtkconv, gboolean first)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleConvChat *chat;

	/*
	 * We know we got something, so we at least have to make sure we don't
	 * send PURPLE_TYPED any time soon.
	 */

	chat = PURPLE_CONV_CHAT(conv);

	purple_conv_chat_stop_send_typed_timeout(chat);
	purple_conv_chat_start_send_typed_timeout(chat);

	/* Check if we need to send another PURPLE_TYPING message */
	if (first || (purple_conv_chat_get_type_again(chat) != 0 && time(NULL) > purple_conv_chat_get_type_again(chat)))
	{
		unsigned int timeout;
		timeout = purple_conv_chat_send_typing(chat, PURPLE_TYPING);
		
		purple_conv_chat_set_type_again(chat, timeout);
	}
}

static void
insert_text_cb(GtkTextBuffer *textbuffer, GtkTextIter *position, gchar *new_text, gint new_text_length, gpointer user_data)
{
	PidginConversation *gtkconv = (PidginConversation *)user_data;

	g_return_if_fail(gtkconv != NULL);

	// TODO should we make a new setting?
	if (!purple_prefs_get_bool("/purple/conversations/im/send_typing"))
		return;

	got_typing_keypress(gtkconv, (gtk_text_iter_is_start(position) && gtk_text_iter_is_end(position)));
}

static void
delete_text_cb(GtkTextBuffer *textbuffer, GtkTextIter *start_pos,
			   GtkTextIter *end_pos, gpointer user_data)
{
	PidginConversation *gtkconv = (PidginConversation *)user_data;
	PurpleConversation *conv;
	PurpleConvChat *chat;

	g_return_if_fail(gtkconv != NULL);

	conv = gtkconv->active_conv;

	if (!purple_prefs_get_bool("/purple/conversations/im/send_typing"))
		return;

	chat = PURPLE_CONV_CHAT(conv);

	if (gtk_text_iter_is_start(start_pos) && gtk_text_iter_is_end(end_pos)) {

		/* We deleted all the text, so turn off typing. */
		purple_conv_chat_stop_send_typed_timeout(chat);
		
		purple_conv_chat_send_typing(chat, PURPLE_NOT_TYPING);
	}
	else {
		/* We're deleting, but not all of it, so it counts as typing. */
		got_typing_keypress(gtkconv, FALSE);
	}
}

static void
connect_typing_signals_to_chat(PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	gboolean chat = (conv->type == PURPLE_CONV_TYPE_CHAT);
	
	if (chat) {
		g_signal_connect(G_OBJECT(gtkconv->entry_buffer), "insert_text", G_CALLBACK(insert_text_cb), gtkconv);
		g_signal_connect(G_OBJECT(gtkconv->entry_buffer), "delete_range", G_CALLBACK(delete_text_cb), gtkconv);
	}
}


// Jabber prpl hijacking

#define JABBER_PRPL_ID "prpl-jabber"

static gulong jabber_chat_conversation_typing_signal = 0;

static guint
jabber_conv_send_typing(PurpleConversation *conv, PurpleTypingState state, gpointer userdata)
{
	PurpleConnection *pc;
	PurplePlugin *plugin;
	PurplePluginProtocolInfo *prpl_info;
	const gchar *chat_state = "active";
	gchar *xml;
	
	pc = purple_conversation_get_gc(conv);

	if (!PURPLE_CONNECTION_IS_CONNECTED(pc)) {
		return 0;
	}

	plugin = purple_connection_get_prpl(pc);
	if (!purple_strequal(purple_plugin_get_id(plugin), JABBER_PRPL_ID)) {
		return 0;
	}
	
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(plugin);
	
	if(state == PURPLE_TYPING)
		chat_state = "composing";
	else if(state == PURPLE_TYPED)
		chat_state = "paused";
	
	//xml as a string, what could go wrong?
	xml = g_strdup_printf("<message type='groupchat' to='%s'><%s xmlns='http://jabber.org/protocol/chatstates'/></message>", purple_conversation_get_name(conv), chat_state);
	prpl_info->send_raw(pc, xml, -1);
	g_free(xml);
	
	return 9999;
}

// Plugin load/unload
 
static void
purple_marshal_UINT__POINTER_UINT(PurpleCallback cb, va_list args, void *data, void **return_val)
{
	guint ret_val;
	void *arg1 = va_arg(args, void *);
	guint arg2 = va_arg(args, guint);

	ret_val = ((guint (*)(void *, guint, void *))cb)(arg1, arg2, data);
	
	if (return_val != NULL)
		*return_val = GUINT_TO_POINTER(ret_val);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_signal_connect(purple_conversations_get_handle(), "chat-buddy-flags", plugin, PURPLE_CALLBACK(grouptyping_chat_buddy_flags_cb), NULL);
	
	purple_signal_connect(pidgin_conversations_get_handle(), "conversation-displayed", plugin, PURPLE_CALLBACK(connect_typing_signals_to_chat), NULL);
	
	
	purple_signal_register(purple_conversations_get_handle(), "chat-conversation-typing",
						 purple_marshal_UINT__POINTER_UINT, 
						 purple_value_new(PURPLE_TYPE_UINT), 2,
						 purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_UINT));
	
	jabber_chat_conversation_typing_signal = purple_signal_connect(purple_conversations_get_handle(), "chat-conversation-typing", plugin, PURPLE_CALLBACK(jabber_conv_send_typing), NULL);
	
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	purple_signals_disconnect_by_handle(plugin);
	purple_signal_unregister(purple_conversations_get_handle(), "chat-conversation-typing");
	
	//purple_signals_disconnect(jabber_chat_conversation_typing_signal);
	
	return TRUE;
}

// static PurplePluginPrefFrame *
// get_plugin_pref_frame(PurplePlugin *plugin) {
	// PurplePluginPrefFrame *frame;
	// PurplePluginPref *ppref;

	// frame = purple_plugin_pref_frame_new();

	// ppref = purple_plugin_pref_new_with_label(_("Conversation Placement"));
	// purple_plugin_pref_frame_add(frame, ppref);

	// ppref = purple_plugin_pref_new_with_label(_("Note: The preference for \"New conversations\" must be set to \"By conversation count\"."));
	// purple_plugin_pref_set_type(ppref, PURPLE_PLUGIN_PREF_INFO);
	// purple_plugin_pref_frame_add(frame, ppref);

	// ppref = purple_plugin_pref_new_with_name_and_label(
							// "/plugins/gtk/extplacement/placement_number",
							// _("Number of conversations per window"));
	// purple_plugin_pref_set_bounds(ppref, 1, 50);
	// purple_plugin_pref_frame_add(frame, ppref);

	// ppref = purple_plugin_pref_new_with_name_and_label(
							// "/plugins/gtk/extplacement/placement_number_separate",
							// _("Separate IM and Chat windows when placing by number"));
	// purple_plugin_pref_frame_add(frame, ppref);

	// return frame;
// }

// static PurplePluginUiInfo prefs_info = {
	// get_plugin_pref_frame,
	// 0,   /* page_num (Reserved) */
	// NULL, /* frame (Reserved) */

	// /* padding */
	// NULL,
	// NULL,
	// NULL,
	// NULL
// };

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	2,
	1,
	PURPLE_PLUGIN_STANDARD,							/**< type			*/
	PIDGIN_PLUGIN_TYPE,								/**< ui_requirement	*/
	0,												/**< flags			*/
	NULL,											/**< dependencies	*/
	PURPLE_PRIORITY_HIGHEST,						/**< priority		*/
	"gtk-grouptyping",								/**< id				*/
	N_("Group Typing Notifications"),								/**< name			*/
	"",									/**< version		*/
	N_("Shows who's typing in a group chat."),	/**< summary		*/
													/**  description	*/
	N_("Requires a compatible prpl"),
	"Eion Robb <eionrobb@gmail.com>",			/**< author			*/
	"",									/**< homepage		*/
	plugin_load,									/**< load			*/
	plugin_unload,									/**< unload			*/
	NULL,											/**< destroy		*/
	NULL,											/**< ui_info		*/
	NULL,											/**< extra_info		*/
	NULL,//&prefs_info,									/**< prefs_info		*/
	NULL,											/**< actions		*/

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	// purple_prefs_add_none("/plugins/gtk/extplacement");
	// purple_prefs_add_int("/plugins/gtk/extplacement/placement_number", 4);
	// purple_prefs_add_bool("/plugins/gtk/extplacement/placement_number_separate", FALSE);
}

PURPLE_INIT_PLUGIN(grouptyping, init_plugin, info)

