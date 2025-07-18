import { NativeModules, Platform } from 'react-native';
import type { Spec } from './NativeAtol';

const LINKING_ERROR =
  `The package 'react-native-atol' doesn't seem to be linked. Make sure: \n\n` +
  Platform.select({ ios: "- You have run 'pod install'\n", default: '' }) +
  '- You rebuilt the app after installing the package\n' +
  '- You are not using Expo Go\n';

// @ts-expect-error
const isTurboModuleEnabled = global.__turboModuleProxy != null;

const AtolModule = isTurboModuleEnabled
  ? require('./NativeAtol').default
  : NativeModules.Atol;

const Atol: Spec = AtolModule
  ? AtolModule
  : new Proxy(
      {},
      {
        get() {
          throw new Error(LINKING_ERROR);
        },
      }
    );

// @ts-ignore
if (!global.__atolModule__) {
  Atol.install();
}

// @ts-ignore
const module = global.__atolModule__;

export function reverseString(str: string): string {
  return module.reverseString(str);
}

export function getNumbers(): Array<number> {
  return module.getNumbers();
}

export function getObject(): Record<string, string> {
  return module.getObject();
}

export function callMeLater(successCB: () => void, failureCB: () => void) {
  module.callMeLater(successCB, failureCB);
}

export function promiseNumber(num: number): Promise<number> {
  return module.promiseNumber(num);
}

export function connect(
  address: string,
  port: string,
  name: string
): Promise<string> {
  return module.connect(address, port, name);
}
