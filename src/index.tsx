import { NativeModules, Platform } from 'react-native';

/**
 * Типы результатов (по соглашению с нативной частью)
 */
export interface ConnectInfo {
  isConnected: boolean;
  modelName?: string;
  shiftState?: number;
  cashSum?: number;
}

export interface ConnectionStatus {
  isConnected: boolean;
}

export interface HeartbeatResult {
  ok: boolean;
}

export interface ShiftStatus {
  shiftState: number;
  shiftNumber: number;
  cashSum: number;
  dateTime: number; // epoch ms
}

export interface OpenShiftResult {
  shiftState: number;
  shiftNumber: number;
  message: string;
}

export interface CloseShiftResult {
  shiftState: number;
  message: string;
}

export interface CashMovementResult {
  cashSum: number;
  message: string;
}

export interface ProcessJsonResult {
  rawResult: string;
  jsonParsable: boolean;
}

export interface SellProductResult {
  rawResult: string;
  printed: boolean;
}

export interface SetDateTimeResult {
  message: string;
}

export interface DisconnectResult {
  disconnected: boolean;
}

export interface XReportResult {
  rc: number;
  errCode: number;
  description: string;
  reportType: string; // "X"
}

const LINKING_ERROR =
  `The package 'react-native-atol' doesn't seem to be linked. Make sure:\n\n` +
  Platform.select({ ios: "- You have run 'pod install'\n", default: '' }) +
  '- You rebuilt the app after installing the package\n' +
  '- You are not using Expo Go\n';

// @ts-expect-error
const isTurboModuleEnabled: boolean = global.__turboModuleProxy != null;

/**
 * Если у вас есть TurboModule Spec (NativeAtol) — используем, иначе fallback.
 * Важно: название экспортируемого модуля в NativeModules должно совпадать (Atol).
 */
// eslint-disable-next-line @typescript-eslint/no-var-requires
const AtolModule: any = isTurboModuleEnabled
  ? require('./NativeAtol').default
  : NativeModules.Atol;

const Atol: any = AtolModule
  ? AtolModule
  : new Proxy(
      {},
      {
        get() {
          throw new Error(LINKING_ERROR);
        },
      }
    );

// Установить JSI модуль, если ещё не установлен
// @ts-ignore
if (!global.__atolModule__) {
  if (typeof Atol.install === 'function') {
    Atol.install();
  } else {
    console.warn(
      '[react-native-atol] install() не найдена. Проверьте экспорт install из нативной части.'
    );
  }
}

// @ts-ignore
const jsiModule = global.__atolModule__;
if (!jsiModule) {
  console.warn(
    '[react-native-atol] __atolModule__ отсутствует. Проверьте успешность инициализации JSI.'
  );
}

/**
 * Ниже – тонкая обёртка над JSI функциями с типами
 */

export function reverseString(str: string): string {
  return jsiModule.reverseString(str);
}

export function getNumbers(): number[] {
  return jsiModule.getNumbers();
}

export function getObject(): Record<string, unknown> {
  return jsiModule.getObject();
}

/**
 * Поддержка старых тестов – если нужно
 * (callMeLater / promiseNumber – вероятно из первоначального примера TurboModule).
 * Если их нет в вашем JSI – оставьте как есть либо удалите.
 */
export function callMeLater(
  successCB: () => void,
  failureCB: () => void
): void {
  if (!jsiModule.callMeLater) {
    throw new Error('callMeLater не реализован в JSI');
  }
  jsiModule.callMeLater(successCB, failureCB);
}

export function promiseNumber(num: number): Promise<number> {
  if (!jsiModule.promiseNumber) {
    return Promise.reject(
      new Error('promiseNumber не реализован в JSI модуле')
    );
  }
  return jsiModule.promiseNumber(num);
}

// -------- Новые методы --------

export function connect(
  address: string,
  port: string,
  name: string
): Promise<ConnectInfo> {
  return jsiModule.connect(address, port, name);
}

export function reconnect(
  address: string,
  port: string,
  name: string
): Promise<ConnectInfo> {
  return jsiModule.reconnect(address, port, name);
}

export function checkConnectionStatus(): Promise<ConnectionStatus> {
  return jsiModule.checkConnectionStatus();
}

export function heartbeat(): Promise<HeartbeatResult> {
  return jsiModule.heartbeat();
}

export function getShiftStatus(): Promise<ShiftStatus> {
  return jsiModule.getShiftStatus();
}

export function openShift(cashierName: string): Promise<OpenShiftResult> {
  return jsiModule.openShift(cashierName);
}

export function closeShift(cashierName: string): Promise<CloseShiftResult> {
  return jsiModule.closeShift(cashierName);
}

export function cashIncome(
  amount: number,
  cashierName: string
): Promise<CashMovementResult> {
  return jsiModule.cashIncome(amount, cashierName);
}

export function cashOutcome(
  amount: number,
  cashierName: string
): Promise<CashMovementResult> {
  return jsiModule.cashOutcome(amount, cashierName);
}

export function processJson(jsonTask: string): Promise<ProcessJsonResult> {
  return jsiModule.processJson(jsonTask);
}

export function sellProduct(jsonTask: string): Promise<SellProductResult> {
  return jsiModule.sellProduct(jsonTask);
}

export function setDateTime(dateTime: string): Promise<SetDateTimeResult> {
  // формат: "yyyy-MM-dd HH:mm:ss"
  return jsiModule.setDateTime(dateTime);
}

export function disconnect(): Promise<DisconnectResult> {
  return jsiModule.disconnect();
}

export function printXReport(): Promise<XReportResult> {
  return jsiModule.printXReport();
}

// Дополнительно: утилита форматирования времени
export function formatEpoch(ms?: number): string {
  if (!ms) return '';
  const d = new Date(ms);
  return (
    d
      .toISOString()
      .replace('T', ' ')
      .replace(/\.\d+Z$/, ' UTC') + ` (${ms})`
  );
}

export default {
  reverseString,
  getNumbers,
  getObject,
  callMeLater,
  promiseNumber,
  connect,
  reconnect,
  checkConnectionStatus,
  heartbeat,
  getShiftStatus,
  openShift,
  closeShift,
  cashIncome,
  cashOutcome,
  processJson,
  sellProduct,
  setDateTime,
  disconnect,
  printXReport,
  formatEpoch,
};
