#include <string.h>
#include <stdio.h>
#include <stdlib.h>
 
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
 
//这里是值不同的流。实际上一个流可以多路同时访问
#define MAX_VIDEO_COUNT        2
#define DEFAULT_RTSP_PORT      "8555"
#define ATTACH_URL             "/test"
 
static const char* VIDEO_FILES[] =
{
    //"/home/quantum6/gh4ai/videos/rtsp-server-1.mov",
    "./1.mp4",
    "./2.mp4",
    NULL
};
 
 
/* called when a stream has received an RTCP packet from the client */
static void on_ssrc_active (GObject * session, GObject * source, GstRTSPMedia * media)
{
  GstStructure *stats;
 
  GST_INFO ("source %p in session %p is active", source, session);
 
  g_object_get (source, "stats", &stats, NULL);
  if (stats) {
    gchar *sstr;
 
    sstr = gst_structure_to_string (stats);
    g_print ("structure: %s\n", sstr);
    g_free (sstr);
 
    gst_structure_free (stats);
  }
}
 
static void on_sender_ssrc_active (GObject * session, GObject * source, GstRTSPMedia * media)
{
  on_ssrc_active(session, source, media);
}
 
/* signal callback when the media is prepared for streaming. We can get the
 * session manager for each of the streams and connect to some signals. */
static void media_prepared_cb (GstRTSPMedia * media)
{
  guint i, n_streams;
 
  n_streams = gst_rtsp_media_n_streams (media);
 
  GST_INFO ("media %p is prepared and has %u streams", media, n_streams);
 
  for (i = 0; i < n_streams; i++) {
    GstRTSPStream *stream;
    GObject *session;
 
    stream = gst_rtsp_media_get_stream (media, i);
    if (stream == NULL)
    {
      continue;
    }
 
    session = gst_rtsp_stream_get_rtpsession (stream);
    GST_INFO ("watching session %p on stream %u", session, i);
 
    g_signal_connect (session, "on-ssrc-active",        (GCallback) on_ssrc_active,        media);
    g_signal_connect (session, "on-sender-ssrc-active", (GCallback) on_sender_ssrc_active, media);
  }
}
 
static void media_configure_cb (GstRTSPMediaFactory * factory, GstRTSPMedia * media)
{
  /* connect our prepared signal so that we can see when this media is
   * prepared for streaming */
  g_signal_connect (media, "prepared", (GCallback) media_prepared_cb, factory);
}
 
int main (int argc, char *argv[])
{
  GMainLoop           *loop    = NULL;
  GstRTSPServer       *server  = NULL;
  GstRTSPMountPoints  *mounts  = NULL;
  GstRTSPMediaFactory *factory = NULL;
  GOptionContext      *optctx  = NULL;
  GError              *error   = NULL;
  gchar               *str     = NULL;
 
  const char          *file    = NULL;
  char temp_buffer[128]        = {0};
  char temp_buffer[128]        = {0};
 
  int count = 0;
  int index = 0;
 
  GOptionEntry entries[] = {
    {"port", 'p', 0, G_OPTION_ARG_STRING, (char*)DEFAULT_RTSP_PORT, NULL, "PORT"},
    {NULL}
  };
 
 
  if (argc >= 2)
  {
      count = atoi(argv[1]);
  }
  if (count <= 0)
  {
      count = MAX_VIDEO_COUNT;
  }
 
  sprintf(temp_buffer, "Port to listen on (default: %s)", DEFAULT_RTSP_PORT);
  entries[0].description = temp_buffer;
 
  optctx = g_option_context_new ("<filename.mp4> - Test RTSP Server, MP4");
  g_option_context_add_main_entries (optctx, entries, NULL);
  g_option_context_add_group (optctx, gst_init_get_option_group ());
 
  if (!g_option_context_parse (optctx, NULL, NULL, &error)) {
    g_printerr ("Error parsing options: %s\n", error->message);
    g_option_context_free (optctx);
    g_clear_error (&error);
    return -1;
  }
  g_option_context_free (optctx);
 
  loop = g_main_loop_new (NULL, FALSE);
 
  server = gst_rtsp_server_new ();
  g_object_set (server, "service", DEFAULT_RTSP_PORT, NULL);
 
  /* get the mount points for this server, every server has a default object
   * that be used to map uri mount points to media factories */
  mounts = gst_rtsp_server_get_mount_points (server);
 
  for (index=0; index<count; index++)
  {
      file = VIDEO_FILES[index];
      //注意，必须是pay0
      str = g_strdup_printf (
              "filesrc location=\"%s\" ! qtdemux "
              "! h264parse ! rtph264pay pt=96 name=pay0 )",
              file);
 
      /* make a media factory for a test stream. The default media factory can use
       * gst-launch syntax to create pipelines. 
       * any launch line works as long as it contains elements named pay%d. Each
       * element with pay%d names will be a stream */
      factory = gst_rtsp_media_factory_new ();
      gst_rtsp_media_factory_set_launch (factory, str);
      g_signal_connect (factory, "media-configure", (GCallback) media_configure_cb, factory);
      g_free (str);
 
      sprintf(temp_buffer, "%s%d", ATTACH_URL, index);
      /* attach the test factory to the /test url */
      gst_rtsp_mount_points_add_factory (mounts, temp_buffer, factory);
      //gst_rtsp_media_factory_set_shared (factory, TRUE);
 
      g_print ("stream ready at rtsp://127.0.0.1:%s%s\n", DEFAULT_RTSP_PORT, temp_buffer);
  }
 
  /* don't need the ref to the mapper anymore */
  g_object_unref (mounts);
  /* attach the server to the default maincontext */
  gst_rtsp_server_attach (server, NULL);
  g_main_loop_run (loop);
  return 0;
}
