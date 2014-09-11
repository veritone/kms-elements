#include <gst/gst.h>
#include "MediaPipeline.hpp"
#include <WebRtcEndpointImplFactory.hpp>
#include "WebRtcEndpointImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>
#include <boost/filesystem.hpp>

#define GST_CAT_DEFAULT kurento_web_rtc_endpoint_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoWebRtcEndpointImpl"

#define FACTORY_NAME "webrtcendpoint"

namespace kurento
{

static const std::string CERTTOOL_TEMPLATE = "autoCerttool.tmpl";
static const std::string CERT_KEY_PEM_FILE = "autoCertkey.pem";

static const uint DEFAULT_STUN_PORT = 0;
static const std::string DEFAULT_STUN_ADDRESS =  "77.72.174.167";

static std::shared_ptr<std::string> pemCertificate;
static std::mutex mutex;

class TemporalDirectory
{
public:
  ~TemporalDirectory() {
    if (!dir.string ().empty() ) {
      boost::filesystem::remove_all (dir);
    }
  }

  void setDir (boost::filesystem::path &dir) {
    this->dir = dir;
  }
private:
  boost::filesystem::path dir;
};

static TemporalDirectory tmpDir;

static void
create_pem_certificate ()
{
  int ret;
  boost::filesystem::path temporalDirectory = boost::filesystem::unique_path (
        boost::filesystem::temp_directory_path() / "WebRtcEndpoint_%%%%%%%%" );
  boost::filesystem::create_directories (temporalDirectory);
  tmpDir.setDir (temporalDirectory);

  boost::filesystem::path pemFile = temporalDirectory / CERT_KEY_PEM_FILE;
  std::string pemGenerationCommand =
    "/bin/sh -c \"certtool --generate-privkey --outfile " + pemFile .string()  +
    "\"";

  ret = system (pemGenerationCommand.c_str() );

  if (ret == -1) {
    return;
  }

  boost::filesystem::path templateFile = temporalDirectory / CERTTOOL_TEMPLATE;
  std::string certtoolCommand = "/bin/sh -c \"echo 'organization = kurento' > " +
                                templateFile.string() + " && certtool --generate-self-signed --load-privkey " +
                                pemFile.string() + " --template " + templateFile.string() +  " >> " +
                                pemFile.string() + " 2>/dev/null\"";

  ret = system (certtoolCommand.c_str() );

  if (ret == -1) {
    return;
  }

  pemCertificate = std::shared_ptr <std::string> (new std::string (
                     pemFile.string() ) );
}

static std::shared_ptr<std::string>
getPemCertificate (const boost::property_tree::ptree &conf)
{
  std::unique_lock<std::mutex> lock (mutex);

  if (pemCertificate) {
    return pemCertificate;
  }

  try {
    boost::filesystem::path pem_certificate_file_name (
      conf.get<std::string> ("modules.kurento.WebRtcEndpoint.pemCertificate") );

    if (pem_certificate_file_name.is_relative() ) {
      pem_certificate_file_name = boost::filesystem::path (
                                    conf.get<std::string> ("configPath") ) / pem_certificate_file_name;
    }

    pemCertificate = std::shared_ptr <std::string> (new std::string (
                       pem_certificate_file_name.string() ) );

    return pemCertificate;
  } catch (boost::property_tree::ptree_error &e) {

  }

  create_pem_certificate();

  return pemCertificate;
}


WebRtcEndpointImpl::WebRtcEndpointImpl (const boost::property_tree::ptree &conf,
                                        std::shared_ptr<MediaPipeline>
                                        mediaPipeline) : SdpEndpointImpl (conf,
                                              std::dynamic_pointer_cast<MediaObjectImpl>
                                              (mediaPipeline), FACTORY_NAME)
{
  uint stunPort;
  std::string stunAddress;
  std::string turnURL;

  //set properties
  try {
    stunPort = conf.get<uint> ("modules.WebRtcEndpoint.stunServerPort");
  } catch (boost::property_tree::ptree_error &e) {
    GST_INFO ("Setting default port %d to stun server",
              DEFAULT_STUN_PORT);
    stunPort = DEFAULT_STUN_PORT;
  }

  if (stunPort != 0) {
    try {
      stunAddress =
        conf.get<std::string> ("modules.WebRtcEndpoint.stunServerAddress");
    } catch (boost::property_tree::ptree_error &e) {
      GST_INFO ("Setting default address %s to stun server",
                DEFAULT_STUN_ADDRESS.c_str() );
      stunAddress = DEFAULT_STUN_ADDRESS;
    }

    if (!stunAddress.empty() ) {
      GST_INFO ("stun port %d\n", stunPort );
      g_object_set ( G_OBJECT (element), "stun-server-port",
                     stunPort, NULL);

      GST_INFO ("stun address %s\n", stunAddress.c_str() );
      g_object_set ( G_OBJECT (element), "stun-server",
                     stunAddress.c_str(),
                     NULL);
    }
  }

  try {
    turnURL = conf.get<std::string> ("modules.WebRtcEndpoint.turnURL");
    GST_INFO ("turn info: %s\n", turnURL.c_str() );
    g_object_set ( G_OBJECT (element), "turn-url", turnURL.c_str(),
                   NULL);
  } catch (boost::property_tree::ptree_error &e) {

  }

  g_object_set ( G_OBJECT (element), "certificate-pem-file",
                 getPemCertificate (conf)->c_str(), NULL);
}

MediaObjectImpl *
WebRtcEndpointImplFactory::createObject (const boost::property_tree::ptree
    &conf, std::shared_ptr<MediaPipeline>
    mediaPipeline) const
{
  return new WebRtcEndpointImpl (conf, mediaPipeline);
}

WebRtcEndpointImpl::StaticConstructor WebRtcEndpointImpl::staticConstructor;

WebRtcEndpointImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
