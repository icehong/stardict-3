#include "stardict_update_info.h"
#include <glib/gi18n.h>

const int my_version_num = 30000000; // As 3,00,00,000, so the version is 3.0.0.0
static int latest_version_num;
static std::string version_msg_title;
static std::string version_msg_content;
static std::string latest_news;

const StarDictPluginSystemService *plugin_service;

static std::string get_cfg_filename()
{
#ifdef _WIN32
	std::string res = g_get_user_config_dir();
	res += G_DIR_SEPARATOR_S "StarDict" G_DIR_SEPARATOR_S "update_info.cfg";
#else
	std::string res;
	gchar *tmp = g_build_filename(g_get_home_dir(), ".stardict", NULL);
	res=tmp;
	g_free(tmp);
	res += G_DIR_SEPARATOR_S "update_info.cfg";
#endif
	return res;
}

static void configure()
{
	std::string content;
	if (latest_version_num > my_version_num) {
		content += _("You are using an old version of StarDict!");
	} else {
		content += _("You are using the newest version of StarDict!");
	}
	content += "\n\n";
	content += _("Latest version information:");
	content += "\n";
	content += version_msg_title;
	content += "\n";
	content += version_msg_content;
	content += "\n\n";
	content += _("Latest news:");
	content += "\n";
	content += latest_news;
	
	GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, content.c_str());
	gtk_window_set_title (GTK_WINDOW (dialog), _("Update information"));
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy (dialog);
}

bool stardict_plugin_init(StarDictPlugInObject *obj)
{
	if (strcmp(obj->version_str, PLUGIN_SYSTEM_VERSION)!=0) {
		g_print("Error: Update info plugin version doesn't match!\n");
		return true;
	}
	obj->type = StarDictPlugInType_MISC;
	obj->info_xml = g_strdup_printf("<plugin_info><name>%s</name><version>1.0</version><short_desc>%s</short_desc><long_desc>%s</long_desc><author>Hu Zheng &lt;huzheng_001@163.com&gt;</author><website>http://stardict.sourceforge.net</website></plugin_info>", _("Update Info"), _("Update information."), _("Get the update information from the Internet."));
	obj->configure_func = configure;
	plugin_service = obj->plugin_service;
	return false;
}

void stardict_plugin_exit(void)
{
}

struct updateinfo_ParseUserData {
	std::string locale_name;
	int latest_version_num;
	std::string version_msg_title;
	std::string version_msg_content;
	std::string latest_news;
};

static void updateinfo_parse_text(GMarkupParseContext *context, const gchar *text, gsize text_len, gpointer user_data, GError **error)
{
	const gchar *element = g_markup_parse_context_get_element(context);
	if (!element)
		return;
	updateinfo_ParseUserData *Data = (updateinfo_ParseUserData *)user_data;
	if (strcmp(element, "latest_version_num")==0) {
		std::string str(text, text_len);
		Data->latest_version_num = atoi(str.c_str());
	} else if (g_str_has_prefix(element, "version_msg_title")) {
		const char *locale = element + (sizeof("version_msg_title")-1);
		if (locale[0] == '\0') {
			if (Data->version_msg_title.empty()) {
				Data->version_msg_title.assign(text, text_len);
			}
		} else if (Data->locale_name == locale+1) {
			Data->version_msg_title.assign(text, text_len);
		}
	} else if (g_str_has_prefix(element, "version_msg_content")) {
		const char *locale = element + (sizeof("version_msg_content")-1);
		if (locale[0] == '\0') {
			if (Data->version_msg_content.empty()) {
				Data->version_msg_content.assign(text, text_len);
			}
		} else if (Data->locale_name == locale+1) {
			Data->version_msg_content.assign(text, text_len);
		}
	} else if (g_str_has_prefix(element, "latest_news")) {
		const char *locale = element + (sizeof("latest_news")-1);
		if (locale[0] == '\0') {
			if (Data->latest_news.empty()) {
				Data->latest_news.assign(text, text_len);
			}
		} else if (Data->locale_name == locale+1) {
			Data->latest_news.assign(text, text_len);
		}
	}
}

static void on_get_http_response(char *buffer, size_t buffer_len, int userdata)
{
	const char *p = g_strstr_len(buffer, buffer_len, "\r\n\r\n");
	if (!p) {
		return;
	}
	p += 4;
	updateinfo_ParseUserData Data;
	Data.latest_version_num = 0;
	const char *locale = g_getenv("LANG");
	if (locale && locale[0] != '\0') {
		const char *p = strchr(locale, '.');
		if (p) {
			Data.locale_name.assign(locale, p - locale);
		} else {
			Data.locale_name = locale;
		}
	}
	GMarkupParser parser;
	parser.start_element = NULL;
	parser.end_element = NULL;
	parser.text = updateinfo_parse_text;
	parser.passthrough = NULL;
	parser.error = NULL;
	GMarkupParseContext* context = g_markup_parse_context_new(&parser, (GMarkupParseFlags)0, &Data, NULL);
	g_markup_parse_context_parse(context, p, buffer_len - (p - buffer), NULL);
	g_markup_parse_context_end_parse(context, NULL);
	g_markup_parse_context_free(context);

	bool updated = false;
	if (Data.latest_version_num != latest_version_num) {
		updated = true;
		latest_version_num = Data.latest_version_num;
		version_msg_title = Data.version_msg_title;
		version_msg_content = Data.version_msg_content;

		if (Data.latest_version_num > my_version_num) {
			std::string content = version_msg_content;
			content += "\n\n";
			content += _("Visit StarDict website now?");
			GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_YES_NO, content.c_str());
			gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
			gtk_window_set_title (GTK_WINDOW (dialog), version_msg_title.c_str());
			if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) {
				plugin_service->show_url("http://stardict.sourceforge.net");
			}
			gtk_widget_destroy (dialog);
		}
	}
	if (Data.latest_news != latest_news) {
		updated = true;
		latest_news = Data.latest_news;

		GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, latest_news.c_str());
		gtk_window_set_title (GTK_WINDOW (dialog), _("Latest News!"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
	if (updated) {
		GKeyFile *keyfile = g_key_file_new();
		g_key_file_set_string(keyfile, "update", "version_msg_title", version_msg_title.c_str());
		g_key_file_set_string(keyfile, "update", "version_msg_content", version_msg_content.c_str());
		g_key_file_set_string(keyfile, "update", "latest_news", latest_news.c_str());
		g_key_file_set_integer(keyfile, "update", "latest_version_num", latest_version_num);
		gsize length;
		gchar *content = g_key_file_to_data(keyfile, &length, NULL);
		std::string res = get_cfg_filename();
		g_file_set_contents(res.c_str(), content, length, NULL);
	}
}

static gboolean get_update_info(gpointer data)
{
	plugin_service->send_http_request("stardict.sourceforge.net", "/UPDATE", on_get_http_response, 0);
	return FALSE;
}

bool stardict_misc_plugin_init(void)
{
	std::string res = get_cfg_filename();
	if (!g_file_test(res.c_str(), G_FILE_TEST_EXISTS)) {
		g_file_set_contents(res.c_str(), "[update]\nlatest_version_num=0\nversion_msg_title=\nversion_msg_content=\nlatest_news=\n", -1, NULL);
	}
	GKeyFile *keyfile = g_key_file_new();
	g_key_file_load_from_file(keyfile, res.c_str(), G_KEY_FILE_NONE, NULL);
	GError *err;
	err = NULL;
	latest_version_num = g_key_file_get_integer(keyfile, "update", "latest_version_num", &err);
	if (err) {
		g_error_free (err);
		latest_version_num = 0;
	}
	char *str;
	str = g_key_file_get_string(keyfile, "update", "version_msg_title", NULL);
	if (str) {
		version_msg_title = str;
		g_free(str);
	}
	str = g_key_file_get_string(keyfile, "update", "version_msg_content", NULL);
	if (str) {
		version_msg_content = str;
		g_free(str);
	}
	str = g_key_file_get_string(keyfile, "update", "latest_news", NULL);
	if (str) {
		latest_news = str;
		g_free(str);
	}
	g_key_file_free(keyfile);
	g_idle_add(get_update_info, NULL);
	g_print(_("Update info plug-in loaded.\n"));
	return false;
}