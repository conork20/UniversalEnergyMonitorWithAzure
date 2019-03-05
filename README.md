# Universal Energy Monitoring With Azure
Monitoring energy usage of any device via Azure using a simple Arduino based Energy Monitor

## Pre-requisite
Windows PowerShell (>= 5.1, `get-host|Select-Object version` to get the version)
1. You need an Azure Subscription and need to have Contributor role on the subscription. You should also have up to date versions of Visual Studio and [.NET Core](https://dotnet.microsoft.com/download) installed.
2. Azure PowerShell Module 
    + [Install Azure Powershell](https://docs.microsoft.com/en-us/powershell/azure/install-az-ps?view=azurermps-6.13.0)
    + Go to Powershell (Admin-role):
      ```Powershell
      Install-Module -Name AzureRM -AllowClobber
      Install-Module -Name AzureAD -AllowClobber
      Install-Module -Name Invoke-MsBuild -AllowClobber
      ```
3. A computer with Git client installed.
4. Install Visual Studio 15.5 (and up)

## Deploying the Azure Components to your Azure Subscription:
1. Clone the repo:

```shell
git clone https://github.com/conork20/UniversalEnergyMonitorWithAzure.git
cd UniversalEnergyMonitorWithAzure\AzureDeployment
```

2. Deploy new service with PowerShell

Launch **new PowerShell session** and connect to Resource Manager and Azure Active Directory with the account that be used as deployment admin.

```PowerShell
Connect-AzureRmAccount
Connect-AzureAD
```

Select subscription where you want to deploy resources.

```PowerShell
Set-AzureRmContext -SubscriptionId <SubscriptionId>
```

Create new resource group and create core resources of the service.

```
.\New-ServiceResources.ps1 -ResourceGroupName <group>
```

## Configuring IoT Central to Receive Telemetry from the Energy Monitor
1. ...

## Configuring the Simulated Device
1. If not already done, create an Azure Function App. See steps [here](https://docs.microsoft.com/en-us/azure/azure-functions/functions-create-first-azure-function)
2. Update [Application Settings in Portal](https://docs.microsoft.com/en-us/azure/azure-functions/functions-how-to-use-azure-function-app-settings#settings) for `voltage` to any integer value, `amperage` to any double value, and `connectionString` to your device's Iot Hub connection string. You can determine your connection string using [this tool](https://github.com/Azure/dps-keygen).
3. Publish [SyntheticSensorMessenger](./SyntheticSensorMessenger) to your Azure Function App. See [Steps for Visual Studio Code](https://code.visualstudio.com/tutorials/functions-extension/deploy-app) or [Steps for Visual Studio](https://blogs.msdn.microsoft.com/benjaminperkins/2018/04/05/deploy-an-azure-function-created-from-visual-studio/)

## Add a Real Device
[Instructions](./ESP8266/README.md)
