
#define PURPLE_PLUGINS

#include "gtkconv.h"
#include "gtkplugin.h"

#ifndef _
#	define _(a) (a)
#	define N_(a) (a)
#endif

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


static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_signal_connect(purple_conversations_get_handle(), "chat-buddy-flags", plugin, PURPLE_CALLBACK(grouptyping_chat_buddy_flags_cb), NULL);
	
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	purple_signals_disconnect_by_handle(plugin);
	
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
	PURPLE_PRIORITY_DEFAULT,						/**< priority		*/
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

