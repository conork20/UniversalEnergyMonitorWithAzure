using System;
using System.Globalization;
using System.Linq;
using System.Text;
using Microsoft.Azure.Devices.Client;
using Microsoft.Azure.WebJobs;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;
using Microsoft.WindowsAzure.Storage;
using Newtonsoft.Json;

namespace SyntheticSensorMessenger
{
    public static class Function1
    {
        [FunctionName("Function1")]
        public static async System.Threading.Tasks.Task RunAsync([TimerTrigger("*/15 * * * * *")] TimerInfo myTimer,
            ILogger log, ExecutionContext context)
        {
            var config = new ConfigurationBuilder()
                .SetBasePath(context.FunctionAppDirectory)
                .AddJsonFile("local.settings.json", optional: true, reloadOnChange: true)
                .AddEnvironmentVariables()
                .Build();

            var now = DateTime.UtcNow;
            try
            {
                // create a usage pattern with some variability
                var lastMinDigit = int.Parse(now.Minute.ToString().Last().ToString());
                var on = lastMinDigit == 1
                         || lastMinDigit == 2
                         || lastMinDigit == 6
                         || lastMinDigit == 7;
                if (now.Hour + now.Month % 3 == 0)
                {
                    on = !on;
                }

                // Get KWH tally
                var storageStr = config["AzureWebJobsStorage"];
                var kwhCounter = 0.0;
                if (!CloudStorageAccount.TryParse(storageStr, out var storageAccount))
                {
                    throw new Exception("Invalid storage connection string");
                }
                var client = storageAccount.CreateCloudBlobClient();
                var container = client.GetContainerReference("counters");
                await container.CreateIfNotExistsAsync();
                var blob = container.GetBlockBlobReference("kwh");
                if (await blob.ExistsAsync())
                {
                    var content = await blob.DownloadTextAsync();
                    kwhCounter = double.Parse(content);
                }

                if (now.Minute == 0 && (now.Second == 0 || now.Second == 1))
                {
                    kwhCounter = 0;
                }

                // Calculate new measurements
                var connectionStr = config["connectionString"];
                var amperage = on ? double.Parse(config["amperage"]) : 0.0;
                var voltage = int.Parse(config["voltage"]);
                var watts = amperage * voltage;
                var kwh = watts * 15.0 / 3600.0;
                var newKwh = kwhCounter + kwh;
                var measurement = new MeasurementMessage
                {
                    TimestampUtc = DateTime.UtcNow,
                    power = amperage * voltage,
                    kwh = newKwh
                };

                // Send measurement
                var message = JsonConvert.SerializeObject(measurement);
                log.LogInformation($"Sending message: {message}");
                using (var deviceClient = DeviceClient.CreateFromConnectionString(connectionStr))
                {
                    var byteArray = Encoding.UTF8.GetBytes(message);
                    var eventMessage = new Message(byteArray);
                    await deviceClient.SendEventAsync(eventMessage);
                }

                //update kwh
                await blob.UploadTextAsync(newKwh.ToString(CultureInfo.InvariantCulture));
            }
            catch (Exception exception)
            {
                log.LogError($"{DateTime.Now} > Exception: {exception.Message}");
                throw;
            }

            log.LogInformation("Message sent to IoT Hub");
            log.LogInformation($"C# Timer trigger function executed at: {DateTime.Now}");
        }
    }

    [Serializable]
    public class MeasurementMessage
    {
        public double power { get; set; }
        public double kwh { get; set; }
        public DateTime TimestampUtc { get; set; }
    }
}
