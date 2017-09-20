namespace DemoBotApp.Controllers
{
    using System;
    using System.Collections.Generic;
    using System.Configuration;
    using System.IO;
    using System.Linq;
    using System.Net;
    using System.Net.Http;
    using System.Threading;
    using System.Threading.Tasks;
    using System.Web;
    using System.Web.Http;
    using DemoBotApp.WebSocket;
    using Microsoft.Bot.Connector.DirectLine;
    using NAudio.Wave;

    [RoutePrefix("chat")]
    public class WebsocketController : ApiController
    {
        private static readonly Uri ShortPhraseUrl = new Uri(Constants.ShortPhraseUrl);
        private static readonly Uri LongDictationUrl = new Uri(Constants.LongPhraseUrl);
        private static readonly Uri SpeechSynthesisUrl = new Uri(Constants.SpeechSynthesisUrl);
        private static readonly string CognitiveSubscriptionKey = ConfigurationManager.AppSettings["CognitiveSubscriptionKey"];

        private TTSClient ttsClient;
        private string speechLocale = Constants.SpeechLocale;

        private DirectLineClient directLineClient;
        private static readonly string DirectLineSecret = ConfigurationManager.AppSettings["DirectLineSecret"];
        private static readonly string BotId = ConfigurationManager.AppSettings["BotId"];
        private static readonly string FromUserId = "TestUser";

        private WebSocketHandler defaultHandler = new WebSocketHandler();
        private static Dictionary<string, WebSocketHandler> handlers = new Dictionary<string, WebSocketHandler>();

        public WebsocketController()
        {
            // Setup bot client
            this.directLineClient = new DirectLineClient(DirectLineSecret);

            // Setup speech synthesis client
            SynthesisOptions synthesisOption = new SynthesisOptions(SpeechSynthesisUrl, CognitiveSubscriptionKey);
            this.ttsClient = new TTSClient(synthesisOption);
        }

        [Route("")]
        [HttpGet]
        public async Task<HttpResponseMessage> Connect(string nickName)
        {
            if (string.IsNullOrEmpty(nickName))
            {
                throw new HttpResponseException(HttpStatusCode.BadRequest);
            }

            WebSocketHandler webSocketHandler = new WebSocketHandler();
            if (handlers.ContainsKey(nickName))
            {
                WebSocketHandler origHandler = handlers[nickName];
                await origHandler.Close();
            }
            handlers[nickName] = webSocketHandler;

            string conversationId = string.Empty;

            webSocketHandler.OnOpened += (sender, arg) =>
            {
                Conversation conversation = this.directLineClient.Conversations.StartConversation();
                conversationId = conversation.ConversationId;

                //this.webSocketHandler.SendMessage($"{nickName} connected! Conversation Id: {conversationId}").Wait();
                //this.webSocketHandler.SendMessage(nickName + " Connected!").Wait();
            };

            webSocketHandler.OnTextMessageReceived += async (sender, message) =>
            {
                await OnMessageReceived(sender, message, conversationId, nickName);
                //this.webSocketHandler.SendMessage($"{nickName} says: {message}").Wait();
                /*
                Activity userMessage = new Activity
                {
                    From = new ChannelAccount(FromUserId, FromUserId),
                    Text = message,
                    Type = ActivityTypes.Message
                };


                directLineClient.Conversations.PostActivity(conversationId, userMessage);
                var botResult = BotClientHelper.ReceiveBotMessages(this.directLineClient, conversationId, null);

                PostVoiceCommandResponse botResponse = new PostVoiceCommandResponse
                {
                    Command = message,
                    Text = botResult.Text,
                    Watermark = botResult.Watermark
                };

                await webSocketHandler.SendMessage($"bot reply: {botResult.Text}");

                // Convert text to speech
                MemoryStream outStream = new MemoryStream();
                byte[] totalBytes;
                if (botResponse.Text.Contains("Music.Play"))
                {
                    totalBytes = ((MemoryStream)SampleMusic.GetStream()).ToArray();
                    await webSocketHandler.SendBinary(totalBytes);
                }
                else
                {
                    this.ttsClient.OnAudioAvailable += (s, stream) =>
                    {
                        WaveFormat target = new WaveFormat(8000, 16, 2);
                        using (WaveFormatConversionStream conversionStream = new WaveFormatConversionStream(target, new WaveFileReader(stream)))
                        {
                            WaveFileWriter.WriteWavFileToStream(outStream, conversionStream);
                            outStream.Position = 0;
                        }

                        stream.Dispose();
                    };

                    totalBytes = ttsClient.SynthesizeText(botResponse.Text);
                }
                */
                /*
                using (MemoryStream ms = (MemoryStream)SampleMusic.GetStream())
                {
                    byte[] totalBytes = ms.ToArray();
                    webSocketHandler.SendBinary(totalBytes).Wait();
                }
                */
            };

            webSocketHandler.OnClosed += (sender, arg) =>
            {
                webSocketHandler.SendMessage(nickName + " Disconnected!").Wait();
                handlers.Remove(nickName);
            };

            HttpContext.Current.AcceptWebSocketRequest(webSocketHandler);
            return Request.CreateResponse(HttpStatusCode.SwitchingProtocols);
        }

        private async Task OnMessageReceived(object sender, string message, string conversationId, string nickName)
        {
            Activity userMessage = new Activity
            {
                From = new ChannelAccount(FromUserId, FromUserId),
                Text = message,
                Type = ActivityTypes.Message
            };


            directLineClient.Conversations.PostActivity(conversationId, userMessage);
            var botResult = BotClientHelper.ReceiveBotMessages(this.directLineClient, conversationId, null);

            PostVoiceCommandResponse botResponse = new PostVoiceCommandResponse
            {
                Command = message,
                Text = botResult.Text,
                Watermark = botResult.Watermark
            };

            await handlers[nickName].SendMessage($"bot reply: {botResult.Text}");

            // Convert text to speech
            MemoryStream outStream = new MemoryStream();
            byte[] totalBytes;
            if (botResponse.Text.Contains("Music.Play"))
            {
                totalBytes = ((MemoryStream)SampleMusic.GetStream()).ToArray();
                await handlers[nickName].SendBinary(totalBytes);
            }
            else
            {
                this.ttsClient.OnAudioAvailable += (s, stream) =>
                {
                    WaveFormat target = new WaveFormat(8000, 16, 2);
                    using (WaveFormatConversionStream conversionStream = new WaveFormatConversionStream(target, new WaveFileReader(stream)))
                    {
                        WaveFileWriter.WriteWavFileToStream(outStream, conversionStream);
                        outStream.Position = 0;
                    }

                    stream.Dispose();
                };

                totalBytes = ttsClient.SynthesizeText(botResponse.Text);
                await ttsClient.SynthesizeTextAsync(botResponse.Text, CancellationToken.None);

                totalBytes = outStream.ToArray();
                await handlers[nickName].SendBinary(totalBytes);
            }
        }
    }
}
