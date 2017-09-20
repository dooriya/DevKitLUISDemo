﻿namespace DemoBotApp
{
    using System;
    using System.Configuration;
    using System.Linq;
    using System.Threading;
    using System.Threading.Tasks;
    using Microsoft.Bot.Connector.DirectLine;

    public static class BotClientHelper
    {
        public static BotMessage ReceiveBotMessages(DirectLineClient client, string conversationId, string watermark)
        {
            bool messageReceived = false;
            BotMessage result = new BotMessage();

            while (!messageReceived)
            {
                var activitySet = client.Conversations.GetActivities(conversationId, watermark);
                result.Watermark = activitySet?.Watermark;

                string botId = ConfigurationManager.AppSettings["BotId"];
                var activities = from x in activitySet.Activities
                                 where x.From.Id == botId
                                 select x;

                if (activities.Count() > 0)
                {
                    messageReceived = true;
                }

                /*
                foreach (Activity activity in activities)
                {
                    result.Text += activity.Text;
                }
                */

                // ONLY return the latest message from bot service here
                result.Text = activities.Last().Text;

                Thread.Sleep(1000);
            }

            return result;
        }

        public static async Task<BotMessage> ReceiveBotMessagesAsync(DirectLineClient client, string conversationId, string watermark)
        {
            bool messageReceived = false;
            BotMessage result = new BotMessage();

            while (!messageReceived)
            {
                var activitySet = await client.Conversations.GetActivitiesAsync(conversationId, watermark);
                result.Watermark = activitySet?.Watermark;

                string botId = ConfigurationManager.AppSettings["BotId"];
                var activities = from x in activitySet.Activities
                                 where x.From.Id == botId
                                 select x;

                if (activities.Count() > 0)
                {
                    messageReceived = true;
                }

                /*
                foreach (Activity activity in activities)
                {
                    result.Text += activity.Text;
                }
                */

                // ONLY return the latest message from bot service here
                result.Text = activities.Last().Text;

                await Task.Delay(TimeSpan.FromSeconds(1)).ConfigureAwait(false);
            }

            return result;
        }
    }
}