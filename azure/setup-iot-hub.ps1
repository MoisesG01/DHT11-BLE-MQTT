# Cria Resource Group, IoT Hub (tier F1) e dispositivo para o bridge ESP32 -> Azure
# Pré-requisito: Azure CLI instalado e logado (az login)
# Uso: .\setup-iot-hub.ps1 -SubscriptionId "sua-subscription-id" -Location "eastus"

param(
    [string]$SubscriptionId = "",
    [string]$Location = "eastus",
    [string]$ResourceGroup = "rg-dht11-esp32",
    [string]$IotHubName = "iothub-dht11-esp32",
    [string]$DeviceId = "esp32-dht11"
)

$ErrorActionPreference = "Stop"

if ($SubscriptionId) {
    az account set --subscription $SubscriptionId
}

Write-Host "Criando Resource Group: $ResourceGroup em $Location ..."
az group create --name $ResourceGroup --location $Location --output none

Write-Host "Criando IoT Hub (tier F1, gratis): $IotHubName ..."
az iot hub create --name $IotHubName --resource-group $ResourceGroup --sku F1 --partition-count 2 --output none

Write-Host "Registrando dispositivo: $DeviceId ..."
az iot hub device-identity create --hub-name $IotHubName --resource-group $ResourceGroup --device-id $DeviceId --output none

$cs = az iot hub device-identity connection-string show --hub-name $IotHubName --device-id $DeviceId --resource-group $ResourceGroup --query connectionString -o tsv
Write-Host ""
Write-Host "=== Connection string do dispositivo (coloque no .env do bridge) ==="
Write-Host $cs
Write-Host ""
Write-Host "Pronto. Agora copie a connection string acima para azure-bridge\.env como IOT_HUB_DEVICE_CONNECTION_STRING"
